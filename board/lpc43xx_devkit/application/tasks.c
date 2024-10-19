/*
 * board/lpc43xx_devkit/application/tasks.c
 * Copyright (C) 2022 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "amplifier.h"
#include "board.h"
#include "interface_proxy.h"
#include "partitions.h"
#include "player.h"
#include "tasks.h"
#include "trace.h"
#include <dpm/audio/codec.h>
#include <dpm/audio/tlv320aic3x.h>
#include <dpm/button_complex.h>
#include <halm/gpio_bus.h>
#include <halm/generic/i2c.h>
#include <halm/generic/mmcsd.h>
#include <halm/timer.h>
#include <halm/watchdog.h>
#include <halm/wq.h>
#include <yaf/fat32.h>
#include <stdio.h>
/*----------------------------------------------------------------------------*/
#define BUS_MAX_RETRIES 100
/*----------------------------------------------------------------------------*/
static void onBusError(void *, void *);
static void onBusIdle(void *, void *);
static void onButtonPlayNextPressed(void *);
static void onButtonPlayPausePressed(void *);
static void onButtonPlayPreviousPressed(void *);
static void onButtonStopPlayingPressed(void *);
static void onButtonSwitchShufflePressed(void *);
static void onCardMounted(void *);
static void onCardUnmounted(void *);
static void onConversionCompleted(void *);
static void onGuardTimerEvent(void *);
static void onMountTimerEvent(void *);
static void onPlayerFormatChanged(void *, uint32_t, uint8_t);
static void onPlayerStateChanged(void *, enum PlayerState);

static void guardCheckTask(void *);
static void mountTask(void *);
static void playNextTask(void *);
static void playPauseTask(void *);
static void playPreviousTask(void *);
static void seedRandomTask(void *);
static void startupTask(void *);
static void stopPlayingTask(void *);
static void switchShuffleTask(void *);
static void unmountTask(void *);
static void volumeChangedTask(void *);

#ifdef ENABLE_DBG
static void debugInfoTask(void *);
static void debugLedsUpdate(void *);
static void onLoadTimerOverflow(void *);
#endif
/*----------------------------------------------------------------------------*/
static void onBusError(void *argument, void *device)
{
  struct Board * const board = argument;

  if (device == board->codecPackage.amp)
  {
    debugTrace("AMP bus error, retry %u",
        (unsigned int)board->event.ampRetries);
  }
  else
  {
    debugTrace("DEC bus error, retry %u",
        (unsigned int)board->event.codecRetries);
  }

  ifSetParam(board->codecPackage.i2c, IF_I2C_BUS_RECOVERY, NULL);

  if (device == board->codecPackage.amp)
  {
    if (board->event.ampRetries < BUS_MAX_RETRIES)
    {
      ++board->event.ampRetries;

      pinReset(board->indication.indA);
      ampReset(board->codecPackage.amp, AMP_GAIN_MIN, false);
    }
  }
  else
  {
    if (board->event.codecRetries < BUS_MAX_RETRIES)
    {
      ++board->event.codecRetries;

      pinReset(board->indication.indB);
      codecReset(board->codecPackage.codec, 44100,
          CODEC_INPUT_PATH, CODEC_OUTPUT_PATH);
    }
  }
}
/*----------------------------------------------------------------------------*/
static void onBusIdle(void *argument, void *device)
{
  struct Board * const board = argument;

  if (device == board->codecPackage.amp)
  {
    board->event.ampRetries = 0;
    pinSet(board->indication.indA);
  }
  else
  {
    if (board->event.codecRetries && !board->event.volume)
    {
      if (wqAdd(WQ_DEFAULT, volumeChangedTask, argument) == E_OK)
        board->event.volume = true;
    }

    board->event.codecRetries = 0;
    pinSet(board->indication.indB);
  }
}
/*----------------------------------------------------------------------------*/
static void onButtonPlayNextPressed(void *argument)
{
  wqAdd(WQ_DEFAULT, playNextTask, argument);
}
/*----------------------------------------------------------------------------*/
static void onButtonPlayPausePressed(void *argument)
{
  wqAdd(WQ_DEFAULT, playPauseTask, argument);
}
/*----------------------------------------------------------------------------*/
static void onButtonPlayPreviousPressed(void *argument)
{
  wqAdd(WQ_DEFAULT, playPreviousTask, argument);
}
/*----------------------------------------------------------------------------*/
static void onButtonStopPlayingPressed(void *argument)
{
  wqAdd(WQ_DEFAULT, stopPlayingTask, argument);
}
/*----------------------------------------------------------------------------*/
static void onButtonSwitchShufflePressed(void *argument)
{
  wqAdd(WQ_DEFAULT, switchShuffleTask, argument);
}
/*----------------------------------------------------------------------------*/
static void onCardMounted(void *argument)
{
  struct Board * const board = argument;

  timerDisable(board->chronoPackage.mountTimer);
  pinSet(board->indication.green);

  playerScanFiles(&board->player, board->fs.handle);

  debugTrace("Card mounted, tracks %lu",
      (unsigned long)playerGetTrackCount(&board->player));
}
/*----------------------------------------------------------------------------*/
static void onCardUnmounted(void *argument)
{
  struct Board * const board = argument;

  playerResetFiles(&board->player);

  pinReset(board->indication.green);
  timerSetValue(board->chronoPackage.mountTimer, 0);
  timerEnable(board->chronoPackage.mountTimer);

  debugTrace("Card unmounted");
}
/*----------------------------------------------------------------------------*/
static void onConversionCompleted(void *argument)
{
  static const int volumeDeltaThreshold = 2;

  struct Board * const board = argument;
  uint16_t sample;

  board->guard.adc = true;
  ifRead(board->analogPackage.adc, &sample, sizeof(sample));

  /* Platform has 10-bit left-aligned ADC */
  if (!board->event.seeded)
  {
    if (board->rng.iteration)
    {
      board->rng.seed = (board->rng.seed << 1) | ((sample >> 6) & 1);
      --board->rng.iteration;
    }

    if (!board->rng.iteration)
    {
      if (wqAdd(WQ_DEFAULT, seedRandomTask, argument) == E_OK)
        board->event.seeded = true;
    }
  }

  const int current = sample >> 8;
  const int previous = board->analogPackage.value;

  if (abs(current - previous) >= volumeDeltaThreshold && !board->event.volume)
  {
    if (wqAdd(WQ_DEFAULT, volumeChangedTask, argument) == E_OK)
    {
      board->event.volume = true;
      board->analogPackage.value = (uint8_t)current;
    }
  }
}
/*----------------------------------------------------------------------------*/
static void onGuardTimerEvent(void *argument)
{
  wqAdd(WQ_LP, guardCheckTask, argument);
}
/*----------------------------------------------------------------------------*/
static void onMountTimerEvent(void *argument)
{
  struct Board * const board = argument;

  /* Card should be mounted after RNG initialization */
  if (board->event.seeded && !board->event.mount && board->fs.handle == NULL)
  {
    if (wqAdd(WQ_DEFAULT, mountTask, board) == E_OK)
      board->event.mount = true;
  }
}
/*----------------------------------------------------------------------------*/
static void onPlayerFormatChanged(void *argument, uint32_t rate,
    [[maybe_unused]] uint8_t channels)
{
  struct Board * const board = argument;

  ifSetParam(board->audio.i2s, IF_RATE, &rate);
  codecSetSampleRate(board->codecPackage.codec, rate);

  debugTrace("Player rate %lu channels %lu",
      (unsigned long)rate, (unsigned long)channels);
}
/*----------------------------------------------------------------------------*/
static void onPlayerStateChanged(void *argument, enum PlayerState state)
{
  struct Board * const board = argument;

#ifdef ENABLE_DBG
  static const char * const stateNameMap[] = {
      "PLAYING", "PAUSED", "STOPPED", "ERROR"
  };
  const size_t index = playerGetCurrentTrack(&board->player);
  const size_t total = playerGetTrackCount(&board->player);
  const char * const name = playerGetTrackName(&board->player);

  debugTrace("Player state %s track %lu/%lu name \"%s\"",
      stateNameMap[state],
      (unsigned long)((total && state != PLAYER_ERROR) ? index + 1 : 0),
      (unsigned long)total,
      name != NULL ? name : ""
  );

  board->debug.state = state;
  debugLedsUpdate(board);
#endif

  switch (state)
  {
    case PLAYER_PLAYING:
      ampReset(board->codecPackage.amp, AMP_GAIN_MAX, true);
      pinReset(board->indication.blue);
      pinSet(board->indication.red);
      break;

    case PLAYER_PAUSED:
      pinSet(board->indication.blue);
      pinReset(board->indication.red);
      break;

    case PLAYER_STOPPED:
      ampReset(board->codecPackage.amp, AMP_GAIN_MIN, false);
      pinReset(board->indication.blue);
      pinReset(board->indication.red);
      break;

    case PLAYER_ERROR:
      ampReset(board->codecPackage.amp, AMP_GAIN_MIN, false);
      pinReset(board->indication.blue);
      pinReset(board->indication.red);
      wqAdd(WQ_DEFAULT, unmountTask, board);
      break;
  }
}
/*----------------------------------------------------------------------------*/
static void guardCheckTask(void *argument)
{
  struct Board * const board = argument;

  /* Check codec state */
  codecCheck(board->codecPackage.codec);

  /* Check task states */
  if (board->guard.adc)
  {
    board->guard.adc = false;

    if (board->system.watchdog != NULL)
      watchdogReload(board->system.watchdog);
  }
}
/*----------------------------------------------------------------------------*/
static void mountTask(void *argument)
{
  struct Board * const board = argument;

  if (board->fs.handle == NULL)
  {
    const struct MMCSDConfig cardConfig = {
        .interface = board->memory.sdmmc,
        .crc = true
    };
    board->memory.card = init(MMCSD, &cardConfig);

    if (board->memory.card != NULL)
    {
      ifSetParam(board->memory.card, IF_BLOCKING, NULL);

      struct InterfaceProxyConfig wrapperConfig = {
          .pipe = board->memory.card
      };
      board->memory.wrapper = init(InterfaceProxy, &wrapperConfig);

      if (board->memory.wrapper != NULL)
      {
        struct PartitionDescriptor partition;

        if (partitionReadHeader(board->memory.wrapper, 0, 0, &partition))
          interfaceProxySetOffset(board->memory.wrapper, partition.offset);

        const struct Fat32Config config = {
            .interface = board->memory.wrapper,
            .nodes = 8,
            .threads = 0
        };
        board->fs.handle = init(FatHandle, &config);

        if (board->fs.handle != NULL)
        {
          onCardMounted(board);
        }
        else
        {
          deinit(board->memory.wrapper);
          board->memory.wrapper = NULL;
          deinit(board->memory.card);
          board->memory.card = NULL;
        }
      }
      else
      {
        deinit(board->memory.card);
        board->memory.card = NULL;
      }
    }
  }

  board->event.mount = false;
}
/*----------------------------------------------------------------------------*/
static void playNextTask(void *argument)
{
  struct Board * const board = argument;
  playerPlayNext(&board->player);
}
/*----------------------------------------------------------------------------*/
static void playPauseTask(void *argument)
{
  struct Board * const board = argument;
  playerPlayPause(&board->player);
}
/*----------------------------------------------------------------------------*/
static void playPreviousTask(void *argument)
{
  struct Board * const board = argument;
  playerPlayPrevious(&board->player);
}
/*----------------------------------------------------------------------------*/
static void seedRandomTask(void *argument)
{
  const struct Board * const board = argument;

  srand(board->rng.seed);
  debugTrace("Seed %lu", (unsigned long)board->rng.seed);
}
/*----------------------------------------------------------------------------*/
static void startupTask(void *argument)
{
  struct Board * const board = argument;

  bhSetErrorCallback(&board->codecPackage.handler, onBusError, board);
  bhSetIdleCallback(&board->codecPackage.handler, onBusIdle, board);
  playerSetControlCallback(&board->player, onPlayerFormatChanged, board);
  playerSetStateCallback(&board->player, onPlayerStateChanged, board);

  ifSetCallback(board->analogPackage.adc, onConversionCompleted, board);

  /* 2 Hz watchdog update task */
  timerSetCallback(board->chronoPackage.guardTimer, onGuardTimerEvent, board);
  timerSetOverflow(board->chronoPackage.guardTimer,
      timerGetFrequency(board->chronoPackage.guardTimer) / 2);
  timerEnable(board->chronoPackage.guardTimer);

  /* 1 Hz card check rate */
  timerSetCallback(board->chronoPackage.mountTimer, onMountTimerEvent, board);
  timerSetOverflow(board->chronoPackage.mountTimer,
      timerGetFrequency(board->chronoPackage.mountTimer));
  timerEnable(board->chronoPackage.mountTimer);

  /* 2 * 100 Hz ADC trigger rate, start ADC sampling */
  ifSetParam(board->analogPackage.adc, IF_ENABLE, NULL);
  timerSetOverflow(board->analogPackage.timer,
      timerGetFrequency(board->analogPackage.timer) / 200);
  timerEnable(board->analogPackage.timer);

  /* Enable base timer for timer factory */
  timerEnable(board->chronoPackage.timer);

  /* Connect and enable buttons */
  buttonComplexSetPressCallback(board->buttonPackage.buttons[0],
      onButtonPlayPreviousPressed, board);
  buttonComplexSetPressCallback(board->buttonPackage.buttons[1],
      onButtonStopPlayingPressed, board);
  buttonComplexSetLongPressCallback(board->buttonPackage.buttons[1],
      onButtonSwitchShufflePressed, board);
  buttonComplexSetPressCallback(board->buttonPackage.buttons[2],
      onButtonPlayPausePressed, board);
  buttonComplexSetPressCallback(board->buttonPackage.buttons[3],
      onButtonPlayNextPressed, board);
  for (size_t i = 0; i < ARRAY_SIZE(board->buttonPackage.buttons); ++i)
    buttonComplexEnable(board->buttonPackage.buttons[i]);

  /* Enqueue power amplifier configuration */
  ampReset(board->codecPackage.amp, AMP_GAIN_MIN, false);
  /* Enqueue audio codec configuration */
  codecReset(board->codecPackage.codec, 44100,
      CODEC_INPUT_PATH, CODEC_OUTPUT_PATH);

  /* Enable SD card power */
  pinSet(board->system.power);

#ifdef ENABLE_DBG
  timerSetCallback(board->debug.timer, onLoadTimerOverflow, board);
  timerSetOverflow(board->debug.timer, timerGetFrequency(board->debug.timer));
  timerEnable(board->debug.timer);

  debugLedsUpdate(board);
#endif
}
/*----------------------------------------------------------------------------*/
static void stopPlayingTask(void *argument)
{
  struct Board * const board = argument;
  playerStopPlaying(&board->player);
}
/*----------------------------------------------------------------------------*/
static void switchShuffleTask(void *argument)
{
  struct Board * const board = argument;

  playerShuffleControl(&board->player, !playerGetShuffleState(&board->player));

  if (board->fs.handle != NULL)
    playerScanFiles(&board->player, board->fs.handle);

  if (board->fs.handle != NULL)
  {
    debugTrace("Shuffle %s, tracks %lu",
        playerGetShuffleState(&board->player) ? "enabled" : "disabled",
        (unsigned long)playerGetTrackCount(&board->player));
  }

#ifdef ENABLE_DBG
  debugLedsUpdate(board);
#endif
}
/*----------------------------------------------------------------------------*/
static void unmountTask(void *argument)
{
  struct Board * const board = argument;

  if (board->fs.handle != NULL)
  {
    deinit(board->fs.handle);
    board->fs.handle = NULL;
    deinit(board->memory.wrapper);
    board->memory.wrapper = NULL;
    deinit(board->memory.card);
    board->memory.card = NULL;

    onCardUnmounted(board);
  }
}
/*----------------------------------------------------------------------------*/
static void volumeChangedTask(void *argument)
{
  struct Board * const board = argument;

  board->event.volume = false;
  codecSetOutputGain(board->codecPackage.codec, CHANNEL_BOTH,
      board->analogPackage.value);

  debugTrace("Volume level %lu", (unsigned long)board->analogPackage.value);
}
/*----------------------------------------------------------------------------*/
#ifdef ENABLE_DBG
static void debugInfoTask(void *argument)
{
  struct Board * const board = argument;

  /* Heap used */
  void * const stub = malloc(0);
  const unsigned int used = (unsigned int)((uintptr_t)stub - (uintptr_t)board);
  free(stub);

  /* CPU used */
  const unsigned int loops = board->debug.loops <= board->debug.idle ?
      board->debug.idle - board->debug.loops : 0;
  const unsigned int load = board->debug.idle > 100 ?
      loops / (board->debug.idle / 100) : 100;

  debugTrace("Heap %u ticks %u cpu %u%%", used, loops, load);
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef ENABLE_DBG
static void debugLedsUpdate(void *argument)
{
  struct Board * const board = argument;
  uint8_t leds = 0;

  if (playerGetShuffleState(&board->player))
    leds |= 0x10;

  if (board->debug.state == PLAYER_PLAYING
      || board->debug.state == PLAYER_PAUSED)
  {
    leds |= 0x20;
  }

  ampSetDebugValue(board->codecPackage.amp, leds);
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef ENABLE_DBG
static void onLoadTimerOverflow(void *argument)
{
  struct Board * const board = argument;

  struct WqInfo info;
  wqStatistics(WQ_DEFAULT, &info);

  board->debug.loops = info.loops;
  wqAdd(WQ_DEFAULT, debugInfoTask, board);
}
#endif
/*----------------------------------------------------------------------------*/
void invokeStartupTask(struct Board *board)
{
  wqAdd(WQ_DEFAULT, startupTask, board);
}

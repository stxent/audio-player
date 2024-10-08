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
#include <dpm/audio/codec.h>
#include <dpm/audio/tlv320aic3x.h>
#include <halm/gpio_bus.h>
#include <halm/generic/mmcsd.h>
#include <halm/interrupt.h>
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
static void startupTask(void *);
static void stopPlayingTask(void *);
static void unmountTask(void *);
static void volumeChangedTask(void *);

#ifdef ENABLE_DBG
static void debugInfoTask(void *);
static void onLoadTimerOverflow(void *);
#endif
/*----------------------------------------------------------------------------*/
static void onBusError(void *argument, void *device)
{
  struct Board * const board = argument;

#ifdef ENABLE_DBG
  size_t count;
  char text[64];

  if (device == board->codecPackage.amp)
  {
    count = sprintf(text, "AMP bus error, retry %u\r\n",
        (unsigned int)board->event.ampRetries);
  }
  else
  {
    count = sprintf(text, "DEC bus error, retry %u\r\n",
        (unsigned int)board->event.codecRetries);
  }

  ifWrite(board->system.serial, text, count);
#endif

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
static void onCardMounted(void *argument)
{
  struct Board * const board = argument;

  timerDisable(board->fs.timer);
  pinSet(board->indication.green);

  playerScanFiles(&board->player, board->fs.handle);

#ifdef ENABLE_DBG
  size_t count;
  char text[64];

  count = sprintf(text, "Card mounted, tracks %lu\r\n",
      (unsigned long)playerGetTrackCount(&board->player));
  ifWrite(board->system.serial, text, count);
#endif
}
/*----------------------------------------------------------------------------*/
static void onCardUnmounted(void *argument)
{
  struct Board * const board = argument;

  playerResetFiles(&board->player);

  pinReset(board->indication.green);
  timerSetValue(board->fs.timer, 0);
  timerEnable(board->fs.timer);
}
/*----------------------------------------------------------------------------*/
static void onConversionCompleted(void *argument)
{
  static const int volumeDeltaThreshold = 2;

  struct Board * const board = argument;
  uint16_t sample;

  board->guard.adc = true;
  ifRead(board->analogPackage.adc, &sample, sizeof(sample));

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

  if (!board->event.mount && board->fs.handle == NULL)
  {
    if (wqAdd(WQ_DEFAULT, mountTask, board) == E_OK)
      board->event.mount = true;
  }
}
/*----------------------------------------------------------------------------*/
static void onPlayerFormatChanged(void *argument, uint32_t rate,
    uint8_t channels)
{
  struct Board * const board = argument;

#ifdef ENABLE_DBG
  size_t count;
  char text[64];

  count = sprintf(text, "Player rate %lu channels %lu\r\n",
      (unsigned long)rate,
      (unsigned long)channels
  );
  ifWrite(board->system.serial, text, count);
#else
  (void)channels;
#endif

  ifSetParam(board->audio.i2s, IF_RATE, &rate);
  codecSetSampleRate(board->codecPackage.codec, rate);
}
/*----------------------------------------------------------------------------*/
static void onPlayerStateChanged(void *argument, enum PlayerState state)
{
  struct Board * const board = argument;

#ifdef ENABLE_DBG
  static const char *stateNameMap[] = {
      "PLAYING", "PAUSED", "STOPPED", "ERROR"
  };
  size_t count;
  const size_t index = playerGetCurrentTrack(&board->player);
  const size_t total = playerGetTrackCount(&board->player);
  const char * const name = playerGetTrackName(&board->player);
  char text[64 + TRACK_PATH_LENGTH];

  count = sprintf(text, "Player state %s track %lu/%lu name \"%s\"\r\n",
      stateNameMap[state],
      (unsigned long)((total && state != PLAYER_ERROR) ? index + 1 : 0),
      (unsigned long)total,
      name != NULL ? name : ""
  );
  ifWrite(board->system.serial, text, count);

  ampSetDebugValue(board->codecPackage.amp,
      (state == PLAYER_PLAYING || state == PLAYER_PAUSED) ? 0x20 : 0x00);
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
static void startupTask(void *argument)
{
  struct Board * const board = argument;

  bhSetErrorCallback(&board->codecPackage.handler, onBusError, board);
  bhSetIdleCallback(&board->codecPackage.handler, onBusIdle, board);
  playerSetControlCallback(&board->player, onPlayerFormatChanged, board);
  playerSetStateCallback(&board->player, onPlayerStateChanged, board);

  ifSetCallback(board->analogPackage.adc, onConversionCompleted, board);

  /* 1 Hz card check rate */
  timerSetCallback(board->fs.timer, onMountTimerEvent, board);
  timerSetOverflow(board->fs.timer,
      timerGetFrequency(board->fs.timer));
  timerEnable(board->fs.timer);

  /* 2 Hz watchdog update task */
  timerSetCallback(board->chronoPackage.guardTimer, onGuardTimerEvent, board);
  timerSetOverflow(board->chronoPackage.guardTimer,
      timerGetFrequency(board->chronoPackage.guardTimer) / 2);
  timerEnable(board->chronoPackage.guardTimer);

  /* 2 * 100 Hz ADC trigger rate, start ADC sampling */
  ifSetParam(board->analogPackage.adc, IF_ENABLE, NULL);
  timerSetOverflow(board->analogPackage.timer,
      timerGetFrequency(board->analogPackage.timer) / 200);
  timerEnable(board->analogPackage.timer);

  /* Enable base timer for timer factory */
  timerEnable(board->chronoPackage.timer);

#ifdef ENABLE_DBG
  timerSetCallback(board->debug.timer, onLoadTimerOverflow, board);
  timerSetOverflow(board->debug.timer, timerGetFrequency(board->debug.timer));
  timerEnable(board->debug.timer);
#endif

  /* Connect and enable buttons */
  interruptSetCallback(board->buttonPackage.buttons[0],
      onButtonPlayPreviousPressed, board);
  interruptSetCallback(board->buttonPackage.buttons[1],
      onButtonStopPlayingPressed, board);
  interruptSetCallback(board->buttonPackage.buttons[2],
      onButtonPlayPausePressed, board);
  interruptSetCallback(board->buttonPackage.buttons[3],
      onButtonPlayNextPressed, board);
  for (size_t i = 0; i < ARRAY_SIZE(board->buttonPackage.buttons); ++i)
    interruptEnable(board->buttonPackage.buttons[i]);

  /* Enqueue power amplifier configuration */
  ampReset(board->codecPackage.amp, AMP_GAIN_MIN, false);
  /* Enqueue audio codec configuration */
  codecReset(board->codecPackage.codec, 44100,
      CODEC_INPUT_PATH, CODEC_OUTPUT_PATH);

  /* Enable SD card power */
  pinSet(board->system.power);
}
/*----------------------------------------------------------------------------*/
static void stopPlayingTask(void *argument)
{
  struct Board * const board = argument;
  playerStopPlaying(&board->player);
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

#ifdef ENABLE_DBG
  size_t count;
  char text[64];

  count = sprintf(text, "Volume level %lu\r\n",
      (unsigned long)board->analogPackage.value);
  ifWrite(board->system.serial, text, count);
#endif
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

  size_t count;
  char text[64];

  count = sprintf(text, "Heap %u ticks %u cpu %u%%\r\n", used, loops, load);
  ifWrite(board->system.serial, text, count);
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

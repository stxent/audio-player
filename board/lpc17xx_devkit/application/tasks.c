/*
 * board/lpc17xx_devkit/application/tasks.c
 * Copyright (C) 2022 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "board.h"
#include "interface_proxy.h"
#include "partitions.h"
#include "player.h"
#include "tasks.h"
#include <halm/gpio_bus.h>
#include <halm/generic/mmcsd.h>
#include <halm/timer.h>
#include <halm/watchdog.h>
#include <halm/wq.h>
#include <yaf/fat32.h>
#include <stdio.h>
/*----------------------------------------------------------------------------*/
static void onButtonCheckEvent(void *);
static void onCardMounted(void *);
static void onCardUnmounted(void *);
static void onConversionCompleted(void *);
static void onMountTimerEvent(void *);
static void onPlayerFormatChanged(void *, uint32_t, uint8_t);
static void onPlayerStateChanged(void *, enum PlayerState);

static void mountTask(void *);
static void playNextTask(void *);
static void playPauseTask(void *);
static void playPreviousTask(void *);
static void seedRandomTask(void *);
static void startupTask(void *);
static void stopPlayingTask(void *);
static void unmountTask(void *);
static void volumeChangedTask(void *);

#ifdef ENABLE_DBG
static void debugInfoTask(void *);
static void onLoadTimerOverflow(void *);
#endif
/*----------------------------------------------------------------------------*/
typedef void (*ButtonCallback)(void *);

static const ButtonCallback BUTTON_TASKS[] = {
    playPreviousTask,
    stopPlayingTask,
    playPauseTask,
    playNextTask
};
/*----------------------------------------------------------------------------*/
static void onButtonCheckEvent(void *argument)
{
  static const uint8_t DEBOUNCE_THRESHOLD = 3;

  struct Board * const board = argument;
  const uint32_t value = gpioBusRead(board->buttonPackage.buttons);

  for (size_t i = 0; i < ARRAY_SIZE(board->buttonPackage.debounce); ++i)
  {
    if (!(value & (1 << i)))
    {
      if (board->buttonPackage.debounce[i] < DEBOUNCE_THRESHOLD)
      {
        if (++board->buttonPackage.debounce[i] == DEBOUNCE_THRESHOLD)
          wqAdd(WQ_DEFAULT, BUTTON_TASKS[i], board);
      }
    }
    else
    {
      if (board->buttonPackage.debounce[i] > 0)
        --board->buttonPackage.debounce[i];
    }
  }

  if (board->system.watchdog != NULL)
    watchdogReload(board->system.watchdog);
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

  count = sprintf(text, "card mounted, tracks %lu\r\n",
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
  struct Board * const board = argument;
  uint16_t sample;

  ifRead(board->analogPackage.adc, &sample, sizeof(sample));
  afAdd(&board->analogPackage.filter, sample >> 4);

  if (!board->event.seeded && afSeedReady(&board->analogPackage.filter))
  {
    if (wqAdd(WQ_DEFAULT, seedRandomTask, argument) == E_OK)
      board->event.seeded = true;
  }

  if (!board->event.volume)
  {
    if (wqAdd(WQ_DEFAULT, volumeChangedTask, argument) == E_OK)
      board->event.volume = true;
  }
}
/*----------------------------------------------------------------------------*/
static void onMountTimerEvent(void *argument)
{
  struct Board * const board = argument;

  /* Card should be mounted after RNG initialization */
  if (board->event.seeded && !board->event.mount && !board->fs.handle)
  {
    if (wqAdd(WQ_DEFAULT, mountTask, board) == E_OK)
      board->event.mount = true;
  }
}
/*----------------------------------------------------------------------------*/
static void onPlayerFormatChanged(void *argument, uint32_t rate,
    uint8_t channels __attribute__((unused)))
{
  struct Board * const board = argument;

  ifSetParam(board->audio.i2s, IF_RATE, &rate);
  codecSetRate(board->codec.codec, rate);
}
/*----------------------------------------------------------------------------*/
static void onPlayerStateChanged(void *argument, enum PlayerState state)
{
  struct Board * const board = argument;

  switch (state)
  {
    case PLAYER_PLAYING:
      pinReset(board->indication.blue);
      pinSet(board->indication.red);
      break;

    case PLAYER_PAUSED:
      pinSet(board->indication.blue);
      pinReset(board->indication.red);
      break;

    case PLAYER_STOPPED:
      pinReset(board->indication.blue);
      pinReset(board->indication.red);
      break;

    case PLAYER_ERROR:
      pinReset(board->indication.blue);
      pinReset(board->indication.red);
      wqAdd(WQ_DEFAULT, unmountTask, board);
      break;
  }

#ifdef ENABLE_DBG
  static const char *STATE_NAMES[] = {
      "PLAYING",
      "PAUSED",
      "STOPPED",
      "ERROR"
  };
  size_t count;
  size_t index = playerGetCurrentTrack(&board->player);
  size_t total = playerGetTrackCount(&board->player);
  char text[64];

  count = sprintf(text, "player state %s track %lu/%lu\r\n",
      STATE_NAMES[state],
      (unsigned long)((total && state != PLAYER_ERROR) ? index + 1 : 0),
      (unsigned long)total
  );
  ifWrite(board->system.serial, text, count);
#endif
}
/*----------------------------------------------------------------------------*/
static void mountTask(void *argument)
{
  struct Board * const board = argument;

  board->event.mount = false;

  if (board->fs.handle == NULL)
  {
    const struct MMCSDConfig cardConfig = {
        .interface = board->memory.sdio,
        .crc = true
    };
    board->memory.card = init(MMCSD, &cardConfig);

    if (board->memory.card)
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
  const uint32_t seed = afSeedValue(&board->analogPackage.filter);

  srand(seed);

#ifdef ENABLE_DBG
  size_t count;
  char text[64];

  count = sprintf(text, "seed %lu\r\n", (unsigned long)seed);
  ifWrite(board->system.serial, text, count);
#endif
}
/*----------------------------------------------------------------------------*/
static void startupTask(void *argument)
{
  struct Board * const board = argument;

  ifSetCallback(board->analogPackage.adc, onConversionCompleted, board);

  playerSetControlCallback(&board->player, onPlayerFormatChanged, board);
  playerSetStateCallback(&board->player, onPlayerStateChanged, board);

  /* 100 Hz button check rate */
  timerSetCallback(board->buttonPackage.timer, onButtonCheckEvent, board);
  timerSetOverflow(board->buttonPackage.timer,
      timerGetFrequency(board->buttonPackage.timer) / 100);
  timerEnable(board->buttonPackage.timer);

  /* 1 Hz card check rate */
  timerSetCallback(board->fs.timer, onMountTimerEvent, board);
  timerSetOverflow(board->fs.timer,
      timerGetFrequency(board->fs.timer));
  timerEnable(board->fs.timer);

  /* 2 * 10 Hz ADC trigger rate, start ADC sampling */
  timerSetOverflow(board->analogPackage.timer,
      timerGetFrequency(board->analogPackage.timer) / 200);
  timerEnable(board->analogPackage.timer);

#ifdef ENABLE_DBG
  timerSetCallback(board->debug.timer, onLoadTimerOverflow, board);
  timerSetOverflow(board->debug.timer, timerGetFrequency(board->debug.timer));
  timerEnable(board->debug.timer);
#endif
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

  if (board->fs.handle)
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
  static const int DELTA_THRESHOLD = 2;

  struct Board * const board = argument;
  const uint16_t average = afValue(&board->analogPackage.filter);
  const int current = (uint8_t)(((int)average * 100) / 4096);
  const int previous = board->analogPackage.value;

  board->event.volume = false;

  if (abs(current - previous) >= DELTA_THRESHOLD)
  {
    board->analogPackage.value = (uint8_t)current;
    codecSetVolume(board->codec.codec, board->analogPackage.value);
  }
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

  count = sprintf(text, "heap %u ticks %u cpu %u%%\r\n", used, loops, load);
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

/*
 * board/lpc43xx_devkit/application/board.c
 * Copyright (C) 2022 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "board.h"
#include "dfu_defs.h"
#include "memory.h"
#include "tasks.h"
#include "trace.h"
#include <dpm/button_complex.h>
#include <halm/core/cortex/nvic.h>
#include <halm/delay.h>
#include <halm/generic/work_queue.h>
#include <halm/platform/lpc/backup_domain.h>
#include <halm/platform/lpc/i2s_dma.h>
#include <halm/timer.h>
#include <halm/watchdog.h>
#include <assert.h>
#include <stdio.h>
/*----------------------------------------------------------------------------*/
#define DFU_BUTTON_MASK 0x00000006UL

enum [[gnu::packed]] InitStep
{
  INIT_CLOCK          = 0,
  INIT_WATCHDOG       = 1,
  INIT_SERIAL         = 2,
  INIT_WORK_QUEUE     = 3,
  INIT_PACKAGE_ANALOG = 4,
  INIT_PACKAGE_CHRONO = 5,
  INIT_PACKAGE_BUTTON = 6,
  INIT_PACKAGE_CODEC  = 7,
  INIT_AUDIO          = 8,
  INIT_MEMORY_SDIO    = 11,
  INIT_DEBUG          = 12,
  INIT_PLAYER         = 13
};
/*----------------------------------------------------------------------------*/
static void panic(struct Board *, enum InitStep);
/*----------------------------------------------------------------------------*/
DECLARE_WQ_IRQ(WQ_LP, SPI_ISR)
/*----------------------------------------------------------------------------*/
static const struct WorkQueueConfig workQueueConfig = {
    .size = 8
};

static const struct WorkQueueIrqConfig workQueueIrqConfig = {
    .size = 2,
    .irq = SPI_IRQ,
    .priority = 0
};
/*----------------------------------------------------------------------------*/
static void panic(struct Board *board, enum InitStep step)
{
  if (board->system.serial != NULL)
  {
    char text[80];
    size_t count;

    count = sprintf(text, "Panic on init step %d\r\n", (unsigned int)step);
    ifWrite(board->system.serial, text, count);
  }

  while (1)
  {
    if (board->system.watchdog != NULL)
      watchdogReload(board->system.watchdog);

    pinToggle(board->indication.red);
    pinToggle(board->indication.green);
    pinToggle(board->indication.blue);
    mdelay(500);
  }
}
/*----------------------------------------------------------------------------*/
void appBoardCheckBoot(struct Board *board)
{
  uint32_t value = 0;

  for (size_t i = 0; i < ARRAY_SIZE(board->buttonPackage.buttons); ++i)
  {
    if (buttonComplexIsPressed(board->buttonPackage.buttons[i]))
      value |= 1 << i;
  }

  if (value == DFU_BUTTON_MASK)
  {
    *(uint32_t *)backupDomainAddress() = DFU_START_REQUEST;
    nvicResetCore();
  }
}
/*----------------------------------------------------------------------------*/
void appBoardInit(struct Board *board)
{
  board->system.serial = NULL;
  board->system.watchdog = NULL;

  const bool ready = boardSetupClock();

  board->system.power = pinInit(BOARD_POWER_PIN);
  pinOutput(board->system.power, false);
  board->indication.blue = pinInit(BOARD_LED_B_PIN);
  pinOutput(board->indication.blue, false);
  board->indication.green = pinInit(BOARD_LED_G_PIN);
  pinOutput(board->indication.green, false);
  board->indication.red = pinInit(BOARD_LED_R_PIN);
  pinOutput(board->indication.red, false);
  board->indication.indA = pinInit(BOARD_LED_WA_PIN);
  pinOutput(board->indication.indA, false);
  board->indication.indB = pinInit(BOARD_LED_WB_PIN);
  pinOutput(board->indication.indB, false);

  if (!ready)
    panic(board, INIT_CLOCK);

#ifdef ENABLE_WDT
  /* Enable watchdog prior to all other peripherals */
  board->system.watchdog = boardMakeWatchdog();
  if (board->system.watchdog == NULL)
    panic(board, INIT_WATCHDOG);
#endif

#ifdef ENABLE_DBG
  board->system.serial = boardMakeSerial();
  if (board->system.serial == NULL)
    panic(board, INIT_SERIAL);
#endif

  /* Initialize Work Queue */
  WQ_DEFAULT = init(WorkQueue, &workQueueConfig);
  if (WQ_DEFAULT == NULL)
    panic(board, INIT_WORK_QUEUE);
  WQ_LP = init(WorkQueueIrq, &workQueueIrqConfig);
  if (WQ_LP == NULL)
    panic(board, INIT_WORK_QUEUE);

  if (!boardSetupAnalogPackage(&board->analogPackage))
    panic(board, INIT_PACKAGE_ANALOG);
  if (!boardSetupChronoPackage(&board->chronoPackage))
    panic(board, INIT_PACKAGE_CHRONO);
  if (!boardSetupButtonPackage(&board->buttonPackage,
      board->chronoPackage.factory))
  {
    panic(board, INIT_PACKAGE_BUTTON);
  }
  if (!boardSetupCodecPackage(&board->codecPackage,
      board->chronoPackage.factory))
  {
    panic(board, INIT_PACKAGE_CODEC);
  }

  board->audio.i2s = boardMakeI2S();
  if (board->audio.i2s == NULL)
    panic(board, INIT_AUDIO);
  board->audio.rx = i2sDmaGetInput((struct I2SDma *)board->audio.i2s);
  board->audio.tx = i2sDmaGetOutput((struct I2SDma *)board->audio.i2s);

  board->fs.handle = NULL;

  board->memory.card = NULL;
  board->memory.wrapper = NULL;
  board->memory.sdmmc = boardMakeSDMMC();
  if (board->memory.sdmmc == NULL)
    panic(board, INIT_MEMORY_SDIO);

  board->event.ampRetries = 0;
  board->event.codecRetries = 0;
  board->event.mount = false;
  board->event.seeded = false;
  board->event.volume = false;

  board->guard.adc = false;

  board->rng.iteration = sizeof(board->rng.seed) * 8;
  board->rng.seed = 0;

  board->debug.idle = 0;
  board->debug.loops = 0;
  board->debug.state = PLAYER_STOPPED;
  board->debug.chrono = boardMakeChronoTimer();
  if (board->debug.chrono == NULL)
    panic(board, INIT_DEBUG);
  board->debug.timer = boardMakeLoadTimer();
  if (board->debug.timer == NULL)
    panic(board, INIT_DEBUG);

  /* Initialize player instance */
  if (!playerInit(&board->player, board->audio.rx, board->audio.tx,
      I2S_BUFFER_COUNT, I2S_RX_BUFFER_LENGTH, I2S_TX_BUFFER_LENGTH, TRACK_COUNT,
      rxBuffers, txBuffers, trackBuffers, rand))
  {
    panic(board, INIT_PLAYER);
  }

  playerShuffleControl(&board->player, true);

#ifdef ENABLE_DBG
  debugTraceInit(board->system.serial, board->debug.chrono);
  timerEnable(board->debug.chrono);
#endif
}
/*----------------------------------------------------------------------------*/
int appBoardStart(struct Board *)
{
  wqStart(WQ_LP);
  wqStart(WQ_DEFAULT);
  return 0;
}

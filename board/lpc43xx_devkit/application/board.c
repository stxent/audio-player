/*
 * board/lpc43xx_devkit/application/board.c
 * Copyright (C) 2022 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "board.h"
#include "memory.h"
#include "tasks.h"
#include <halm/core/cortex/nvic.h>
#include <halm/generic/work_queue.h>
#include <halm/platform/lpc/i2s_dma.h>
#include <halm/timer.h>
#include <assert.h>
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
void appBoardInit(struct Board *board)
{
  bool ready = boardSetupClock();

#ifdef ENABLE_WDT
  /* Enable watchdog prior to all other peripherals */
  board->system.watchdog = boardMakeWatchdog();
  assert(board->system.watchdog != NULL);
#else
  board->system.watchdog = NULL;
#endif

#ifdef ENABLE_DBG
  board->system.serial = boardMakeSerial();
  assert(board->system.serial != NULL);
#else
  board->system.serial = NULL;
#endif

  /* Initialize Work Queue */
  WQ_DEFAULT = init(WorkQueue, &workQueueConfig);
  assert(WQ_DEFAULT != NULL);
  WQ_LP = init(WorkQueueIrq, &workQueueIrqConfig);
  assert(WQ_LP != NULL);

  board->system.power = pinInit(BOARD_POWER_PIN);
  pinOutput(board->system.power, false);
  board->indication.blue = pinInit(BOARD_LED_B_PIN);
  pinOutput(board->indication.blue, false);
  board->indication.green = pinInit(BOARD_LED_G_PIN);
  pinOutput(board->indication.green, false);
  board->indication.red = pinInit(BOARD_LED_R_PIN);
  pinOutput(board->indication.red, false);
  board->indication.whiteA = pinInit(BOARD_LED_WA_PIN);
  pinOutput(board->indication.whiteA, true);
  board->indication.whiteB = pinInit(BOARD_LED_WB_PIN);
  pinOutput(board->indication.whiteB, true);

  ready = ready && boardSetupAnalogPackage(&board->analogPackage);
  ready = ready && boardSetupButtonPackage(&board->buttonPackage);
  ready = ready && boardSetupCodecPackage(&board->codecPackage);

  board->audio.i2s = boardMakeI2S();
  assert(board->audio.i2s != NULL);
  board->audio.rx = i2sDmaGetInput((struct I2SDma *)board->audio.i2s);
  board->audio.tx = i2sDmaGetOutput((struct I2SDma *)board->audio.i2s);

  board->fs.handle = NULL;
  board->fs.timer = boardMakeMountTimer();
  assert(board->fs.timer != NULL);

  board->memory.card = NULL;
  board->memory.wrapper = NULL;
  board->memory.sdmmc = boardMakeSDMMC();
  assert(board->memory.sdmmc != NULL);

  board->event.ampRetries = 0;
  board->event.codecRetries = 0;
  board->event.mount = false;
  board->event.volume = false;

  board->debug.idle = 0;
  board->debug.loops = 0;
  board->debug.timer = boardMakeLoadTimer();
  assert(board->debug.timer != NULL);

  /* Init player instance */
  ready = ready && playerInit(&board->player, board->audio.rx, board->audio.tx,
      I2S_BUFFER_COUNT, I2S_RX_BUFFER_LENGTH, I2S_TX_BUFFER_LENGTH,
      rxBuffers, txBuffers, TRACK_COUNT, NULL);

  assert(ready);
  (void)ready;
}
/*----------------------------------------------------------------------------*/
int appBoardStart(struct Board *board __attribute__((unused)))
{
  wqStart(WQ_LP);
  wqStart(WQ_DEFAULT);
  return 0;
}

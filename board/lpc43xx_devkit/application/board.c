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
  boardSetupClock();

#ifdef ENABLE_WDT
  /* Enable watchdog prior to all other peripherals */
  board->system.watchdog = boardMakeWatchdog();
#else
  board->system.watchdog = NULL;
#endif

#ifdef ENABLE_DBG
  board->system.serial = boardMakeSerial();
#else
  board->system.serial = NULL;
#endif

  /* Initialize Work Queue */
  WQ_DEFAULT = init(WorkQueue, &workQueueConfig);
  assert(WQ_DEFAULT);
  WQ_LP = init(WorkQueueIrq, &workQueueIrqConfig);
  assert(WQ_LP != NULL);

  board->indication.blue = pinInit(BOARD_LED_B_PIN);
  pinOutput(board->indication.blue, false);
  board->indication.green = pinInit(BOARD_LED_G_PIN);
  pinOutput(board->indication.green, false);
  board->indication.red = pinInit(BOARD_LED_R_PIN);
  pinOutput(board->indication.red, false);
  board->indication.white = pinInit(BOARD_LED_W_PIN);
  pinOutput(board->indication.white, false);

  boardSetupAnalogPackage(&board->analogPackage);
  boardSetupButtonPackage(&board->buttonPackage);

  board->audio.i2s = boardMakeI2S();
  board->audio.rx = i2sDmaGetInput((struct I2SDma *)board->audio.i2s);
  board->audio.tx = i2sDmaGetOutput((struct I2SDma *)board->audio.i2s);

  board->codec.i2c = boardMakeI2C();
  board->codec.timer = boardMakeCodecTimer();
  board->codec.codec = boardMakeCodec(board->codec.i2c, board->codec.timer);

  board->fs.handle = NULL;
  board->fs.timer = boardMakeMountTimer();

  board->memory.card = NULL;
  board->memory.sdmmc = boardMakeSDMMC();
  board->memory.wrapper = NULL;

  board->event.mount = false;
  board->event.volume = false;

  board->debug.timer = boardMakeLoadTimer();
  board->debug.idle = 0;
  board->debug.loops = 0;

  /* Init player instance */
  playerInit(&board->player, board->audio.rx, board->audio.tx,
      I2S_BUFFER_COUNT, I2S_RX_BUFFER_LENGTH, I2S_TX_BUFFER_LENGTH,
      rxBuffers, txBuffers, NULL);
}
/*----------------------------------------------------------------------------*/
int appBoardStart(struct Board *board __attribute__((unused)))
{
  wqStart(WQ_LP);
  wqStart(WQ_DEFAULT);
  return 0;
}

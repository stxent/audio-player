/*
 * board/lpc17xx_devkit/application/board.c
 * Copyright (C) 2022 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "board.h"
#include "memory.h"
#include "tasks.h"
#include <halm/generic/work_queue.h>
#include <halm/platform/lpc/i2s_dma.h>
#include <halm/timer.h>
#include <assert.h>
/*----------------------------------------------------------------------------*/
static const struct WorkQueueConfig workQueueConfig = {
    .size = 4
};
/*----------------------------------------------------------------------------*/
void appBoardInit(struct Board *board)
{
  boardSetupClock();

  /* Initialize Work Queue */
  WQ_DEFAULT = init(WorkQueue, &workQueueConfig);
  assert(WQ_DEFAULT);

  board->indication.blue = pinInit(BOARD_LED_B_PIN);
  pinOutput(board->indication.blue, false);
  board->indication.green = pinInit(BOARD_LED_G_PIN);
  pinOutput(board->indication.green, false);
  board->indication.red = pinInit(BOARD_LED_R_PIN);
  pinOutput(board->indication.red, false);
  board->indication.serial = boardMakeSerial();

  boardSetupAnalogPackage(&board->analogPackage);
  boardSetupButtonPackage(&board->buttonPackage);

  board->audio.i2s = boardMakeI2S();
  board->audio.rx = i2sDmaGetInput((struct I2SDma *)board->audio.i2s);
  board->audio.tx = i2sDmaGetOutput((struct I2SDma *)board->audio.i2s);

  board->codec.i2c = boardMakeI2C();
  board->codec.codec = boardMakeCodec(board->codec.i2c);

  board->fs.handle = 0;
  board->fs.timer = boardMakeMountTimer();

  board->memory.timer = boardMakeMemoryTimer();
  timerSetOverflow(board->memory.timer, 200); /* 5 kHz event rate */

  board->memory.card = 0;
  board->memory.spi = boardMakeSPI();
  board->memory.sdio = boardMakeSDIO(board->memory.spi, board->memory.timer);
  board->memory.wrapper = 0;

  board->event.volume = false;

  board->debug.timer = boardMakeLoadTimer();
  board->debug.idle = 0;
  board->debug.loops = 0;

  /* Init player instance */
  playerInit(&board->player, board->audio.rx, board->audio.tx,
      I2S_BUFFER_COUNT, I2S_RX_BUFFER_LENGTH, I2S_TX_BUFFER_LENGTH,
      rxBuffers, txBuffers);
}
/*----------------------------------------------------------------------------*/
int appBoardStart(struct Board *board __attribute__((unused)))
{
  wqStart(WQ_DEFAULT);
  return 0;
}

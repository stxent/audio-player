/*
 * board/lpc17xx_devkit/application/board.c
 * Copyright (C) 2022 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "board.h"
#include "dfu_defs.h"
#include "memory.h"
#include "tasks.h"
#include "trace.h"
#include <halm/core/cortex/nvic.h>
#include <halm/generic/work_queue.h>
#include <halm/gpio_bus.h>
#include <halm/platform/lpc/backup_domain.h>
#include <halm/platform/lpc/i2s_dma.h>
#include <halm/timer.h>
#include <assert.h>
/*----------------------------------------------------------------------------*/
#define DFU_BUTTON_MASK 0x00000006UL

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
void appBoardCheckBoot(struct Board *board)
{
  const uint32_t value = gpioBusRead(board->buttonPackage.buttons);

  if (!(value & DFU_BUTTON_MASK))
  {
    *(uint32_t *)backupDomainAddress() = DFU_START_REQUEST;
    nvicResetCore();
  }
}
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
  debugTraceInit(board->system.serial, NULL);
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
  board->indication.indA = pinInit(BOARD_LED_IND_A_PIN);
  pinOutput(board->indication.indA, false);
  board->indication.indB = pinInit(BOARD_LED_IND_B_PIN);
  pinOutput(board->indication.indB, false);

  ready = ready && boardSetupAnalogPackage(&board->analogPackage);
  ready = ready && boardSetupButtonPackage(&board->buttonPackage);
  ready = ready && boardSetupChronoPackage(&board->chronoPackage);
  ready = ready && boardSetupCodecPackage(&board->codecPackage,
      board->chronoPackage.factory);

  board->audio.i2s = boardMakeI2S();
  assert(board->audio.i2s != NULL);
  board->audio.rx = i2sDmaGetInput((struct I2SDma *)board->audio.i2s);
  board->audio.tx = i2sDmaGetOutput((struct I2SDma *)board->audio.i2s);

  board->fs.handle = NULL;
  board->fs.timer = boardMakeMountTimer();
  assert(board->fs.timer != NULL);

  board->memory.timer = boardMakeMemoryTimer();
  assert(board->memory.timer != NULL);
  timerSetOverflow(board->memory.timer, 200); /* 5 kHz event rate */

  board->memory.card = NULL;
  board->memory.wrapper = NULL;
  board->memory.spi = boardMakeSPI();
  assert(board->memory.spi != NULL);
  board->memory.sdio = boardMakeSDIO(board->memory.spi, board->memory.timer);
  assert(board->memory.sdio != NULL);

  board->event.ampRetries = 0;
  board->event.codecRetries = 0;
  board->event.mount = false;
  board->event.seeded = false;
  board->event.volume = false;

  board->guard.adc = false;
  board->guard.button = false;

  board->debug.idle = 0;
  board->debug.loops = 0;
  board->debug.timer = boardMakeLoadTimer();
  assert(board->debug.timer != NULL);

  /* Initialize player instance */
  ready = ready && playerInit(&board->player, board->audio.rx, board->audio.tx,
      I2S_BUFFER_COUNT, I2S_RX_BUFFER_LENGTH, I2S_TX_BUFFER_LENGTH, TRACK_COUNT,
      rxBuffers, txBuffers, trackBuffers, rand);

  assert(ready);
  (void)ready;

  playerShuffleControl(&board->player, true);
}
/*----------------------------------------------------------------------------*/
int appBoardStart(struct Board *)
{
  wqStart(WQ_LP);
  wqStart(WQ_DEFAULT);
  return 0;
}

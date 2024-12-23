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
#include <halm/generic/work_queue.h>
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
  [[maybe_unused]] bool ready = boardSetupClock();

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
  board->indication.indA = pinInit(BOARD_LED_WA_PIN);
  pinOutput(board->indication.indA, false);
  board->indication.indB = pinInit(BOARD_LED_WB_PIN);
  pinOutput(board->indication.indB, false);

  ready = ready && boardSetupAnalogPackage(&board->analogPackage);
  ready = ready && boardSetupChronoPackage(&board->chronoPackage);
  ready = ready && boardSetupButtonPackage(&board->buttonPackage,
      board->chronoPackage.factory);
  ready = ready && boardSetupCodecPackage(&board->codecPackage,
      board->chronoPackage.factory);

  board->audio.i2s = boardMakeI2S();
  assert(board->audio.i2s != NULL);
  board->audio.rx = i2sDmaGetInput((struct I2SDma *)board->audio.i2s);
  board->audio.tx = i2sDmaGetOutput((struct I2SDma *)board->audio.i2s);

  board->fs.handle = NULL;

  board->memory.card = NULL;
  board->memory.wrapper = NULL;
  board->memory.sdmmc = boardMakeSDMMC();
  assert(board->memory.sdmmc != NULL);

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
  assert(board->debug.chrono != NULL);
  board->debug.timer = boardMakeLoadTimer();
  assert(board->debug.timer != NULL);

  /* Initialize player instance */
  ready = ready && playerInit(&board->player, board->audio.rx, board->audio.tx,
      I2S_BUFFER_COUNT, I2S_RX_BUFFER_LENGTH, I2S_TX_BUFFER_LENGTH, TRACK_COUNT,
      rxBuffers, txBuffers, trackBuffers, rand);
  assert(ready);

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

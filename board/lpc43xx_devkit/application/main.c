/*
 * board/lpc43xx_devkit/application/main.c
 * Copyright (C) 2022 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "accel.h"
#include "board.h"
#include "tasks.h"
#include "trace.h"
#include <halm/core/cortex/nvic.h>
#include <halm/platform/lpc/clocking.h>
#include <xcore/interface.h>
#include <stdio.h>
#include <stdlib.h>
/*----------------------------------------------------------------------------*/
#if defined(ENABLE_NOR) && defined(ENABLE_DBG)
static void showClockFrequencies(void)
{
  uint32_t frequency;

  frequency = clockFrequency(ExternalOsc);
  debugTrace("XTAL  %lu", (unsigned long)frequency);

  frequency = clockFrequency(MainClock);
  debugTrace("MAIN  %lu", (unsigned long)frequency);

  frequency = clockFrequency(SdioClock);
  debugTrace("SDMMC %lu", (unsigned long)frequency);

  frequency = clockFrequency(SpifiClock);
  debugTrace("SPIFI %lu", (unsigned long)frequency);

  frequency = clockFrequency(SystemPll);
  debugTrace("PLL   %lu", (unsigned long)frequency);
}
#endif
/*----------------------------------------------------------------------------*/
int main(void)
{
  loadAcceleratedCode();

  struct Board * const board = malloc(sizeof(struct Board));
  appBoardInit(board);

#ifdef ENABLE_DFU
  appBoardCheckBoot(board);
#endif

#ifdef ENABLE_DBG
  /*
   * Calculate estimate idle cycles. Test results are:
   *   20399993 for 204 MHz
   *   10199993 for 102 MHz
   *    5099992 for  51 MHz
   */
  board->debug.idle = clockFrequency(MainClock) / 10;

#  ifdef ENABLE_NOR
  showClockFrequencies();
#  endif
#endif /* ENABLE_DBG */

  invokeStartupTask(board);
  return appBoardStart(board);
}

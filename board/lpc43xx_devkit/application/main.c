/*
 * board/lpc43xx_devkit/application/main.c
 * Copyright (C) 2022 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "board.h"
#include "tasks.h"
#include <halm/core/cortex/nvic.h>
#include <halm/platform/lpc/clocking.h>
#include <xcore/interface.h>
#include <stdio.h>
#include <stdlib.h>
/*----------------------------------------------------------------------------*/
extern unsigned long _saccel;
extern unsigned long _eaccel;
extern unsigned long _siaccel;
/*----------------------------------------------------------------------------*/
#if defined(ENABLE_NOR)
static void loadAcceleratedCode(void)
{
  register unsigned long *dst __asm__ ("r0");
  register unsigned long *src __asm__ ("r1");

  /* Copy the accelerated code from Flash to RAM */
  for (dst = &_saccel, src = &_siaccel; dst < &_eaccel;)
    *dst++ = *src++;
}
#endif
/*----------------------------------------------------------------------------*/
#if defined(ENABLE_NOR) && defined(ENABLE_DBG)
static void showClockFrequencies(struct Interface *serial)
{
  uint32_t frequency;
  size_t count;
  char text[64];

  frequency = clockFrequency(ExternalOsc);
  count = sprintf(text, "XTAL  %lu\r\n", (unsigned long)frequency);
  ifWrite(serial, text, count);

  frequency = clockFrequency(MainClock);
  count = sprintf(text, "MAIN  %lu\r\n", (unsigned long)frequency);
  ifWrite(serial, text, count);

  frequency = clockFrequency(SdioClock);
  count = sprintf(text, "SDMMC %lu\r\n", (unsigned long)frequency);
  ifWrite(serial, text, count);

  frequency = clockFrequency(SpifiClock);
  count = sprintf(text, "SPIFI %lu\r\n", (unsigned long)frequency);
  ifWrite(serial, text, count);

  frequency = clockFrequency(SystemPll);
  count = sprintf(text, "PLL   %lu\r\n", (unsigned long)frequency);
  ifWrite(serial, text, count);
}
#endif
/*----------------------------------------------------------------------------*/
int main(void)
{
#ifdef ENABLE_NOR
  loadAcceleratedCode();
#endif

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
  showClockFrequencies(board->system.serial);
#  endif
#endif /* ENABLE_DBG */

  invokeStartupTask(board);
  return appBoardStart(board);
}

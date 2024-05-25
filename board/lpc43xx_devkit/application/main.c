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
extern unsigned long _slibs;
extern unsigned long _elibs;
extern unsigned long _silibs;
/*----------------------------------------------------------------------------*/
#if defined(ENABLE_NOR)
static void loadLibraryCode(void)
{
  register unsigned long *dst __asm__ ("r0");
  register unsigned long *src __asm__ ("r1");

  /* Copy the library segment initializers from flash to RAM */
  for (dst = &_slibs, src = &_silibs; dst < &_elibs;)
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
  loadLibraryCode();
  nvicSetVectorTableOffset((uint32_t)&_slibs);
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

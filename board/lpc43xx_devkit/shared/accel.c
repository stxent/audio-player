/*
 * board/lpc43xx_devkit/shared/accel.c
 * Copyright (C) 2024 xent
 * Project is distributed under the terms of the MIT License
 */

#include "accel.h"
/*----------------------------------------------------------------------------*/
extern unsigned long _saccel;
extern unsigned long _eaccel;
extern unsigned long _siaccel;
/*----------------------------------------------------------------------------*/
#ifdef ENABLE_NOR
void loadAcceleratedCode(void)
{
  register unsigned long *dst __asm__ ("r0");
  register unsigned long *src __asm__ ("r1");

  /* Copy the accelerated code from Flash to RAM */
  for (dst = &_saccel, src = &_siaccel; dst < &_eaccel;)
    *dst++ = *src++;
}
#endif

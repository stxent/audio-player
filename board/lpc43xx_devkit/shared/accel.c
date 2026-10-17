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
  unsigned long *dst;
  unsigned long *src;

  /* Copy the accelerated code from Flash to RAM */
  for (dst = &_saccel, src = &_siaccel; dst < &_eaccel;)
    *dst++ = *src++;
}
#endif

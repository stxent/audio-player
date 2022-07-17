/*
 * board/lpc17xx_devkit/application/main.c
 * Copyright (C) 2022 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "board.h"
#include "tasks.h"
#include <xcore/interface.h>
#include <stdio.h>
#include <stdlib.h>
/*----------------------------------------------------------------------------*/
int main(void)
{
  struct Board * const board = malloc(sizeof(struct Board));
  appBoardInit(board);
  invokeStartupTask(board);

#ifdef ENABLE_DBG
  board->debug.idle = 9999989;
#endif

  return appBoardStart(board);
}

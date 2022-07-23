/*
 * board/lpc43xx_devkit/application/tasks.h
 * Copyright (C) 2022 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef BOARD_LPC43XX_DEVKIT_APPLICATION_TASKS_H_
#define BOARD_LPC43XX_DEVKIT_APPLICATION_TASKS_H_
/*----------------------------------------------------------------------------*/
#include <xcore/helpers.h>
/*----------------------------------------------------------------------------*/
struct Board;
/*----------------------------------------------------------------------------*/
BEGIN_DECLS

void invokeStartupTask(struct Board *);

END_DECLS
/*----------------------------------------------------------------------------*/
#endif /* BOARD_LPC43XX_DEVKIT_APPLICATION_TASKS_H_ */

/*
 * board/lpc17xx_devkit/application/memory.h
 * Copyright (C) 2022 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef BOARD_LPC17XX_DEVKIT_APPLICATION_MEMORY_H_
#define BOARD_LPC17XX_DEVKIT_APPLICATION_MEMORY_H_
/*----------------------------------------------------------------------------*/
#include <stdint.h>
/*----------------------------------------------------------------------------*/
#define I2S_BUFFER_COUNT      3
#define I2S_RX_BUFFER_LENGTH  2048
#define I2S_TX_BUFFER_LENGTH  4608
#define TRACK_COUNT           128

extern void *trackBuffers;
extern void *rxBuffers;
extern void *txBuffers;
/*----------------------------------------------------------------------------*/
#endif /* BOARD_LPC17XX_DEVKIT_APPLICATION_MEMORY_H_ */

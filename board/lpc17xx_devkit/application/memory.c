/*
 * board/lpc17xx_devkit/application/memory.c
 * Copyright (C) 2022 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "memory.h"
/*----------------------------------------------------------------------------*/
typedef uint8_t I2SRxBuffer[I2S_RX_BUFFER_LENGTH];
typedef uint8_t I2STxBuffer[I2S_TX_BUFFER_LENGTH];
/*----------------------------------------------------------------------------*/
// TODO I2S RX
// /* Total: 2048 bytes */
// I2SRxBuffer rxBuffers[I2S_BUFFER_COUNT] __attribute__((section(".sram2")));

void *rxBuffers = 0;
/*----------------------------------------------------------------------------*/
/* Total: 18432 bytes */
I2STxBuffer txBuffersData[I2S_BUFFER_COUNT] __attribute__((section(".sram1")));
void *txBuffers = txBuffersData;

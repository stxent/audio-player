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
// #ifdef CONFIG_ENABLE_MP3
// I2SBuffer rxBuffers = 0;
// #else
// /* Total: 16384 bytes */
// I2SBuffer rxBuffers[I2S_BUFFER_COUNT] __attribute__((section(".sram2")));
// #endif

void *rxBuffers = 0;
/*----------------------------------------------------------------------------*/
/* Total: 18432 bytes */
I2STxBuffer txBuffersData[I2S_BUFFER_COUNT] __attribute__((section(".sram1")));
void *txBuffers = txBuffersData;

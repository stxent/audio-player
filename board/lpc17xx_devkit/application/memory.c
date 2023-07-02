/*
 * board/lpc17xx_devkit/application/memory.c
 * Copyright (C) 2022 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "memory.h"
#include "player.h"
/*----------------------------------------------------------------------------*/
typedef uint8_t I2SRxBuffer[I2S_RX_BUFFER_LENGTH];
typedef uint8_t I2STxBuffer[I2S_TX_BUFFER_LENGTH];
/*----------------------------------------------------------------------------*/
/* Total: 6144 bytes */
static I2SRxBuffer rxBuffersData[I2S_BUFFER_COUNT];
void *rxBuffers = rxBuffersData;
/*----------------------------------------------------------------------------*/
/* Total: 13824 bytes */
static I2STxBuffer txBuffersData[I2S_BUFFER_COUNT]
    __attribute__((section(".sram1")));
void *txBuffers = txBuffersData;
/*----------------------------------------------------------------------------*/
/* Total: 8192 bytes */
static FilePath trackBufferData[TRACK_COUNT]
    __attribute__((section(".sram2")));
void *trackBuffers = trackBufferData;

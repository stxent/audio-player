/*
 * board/lpc43xx_devkit/application/memory.c
 * Copyright (C) 2022 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "memory.h"
#include "player.h"
/*----------------------------------------------------------------------------*/
typedef uint8_t I2SRxBuffer[I2S_RX_BUFFER_LENGTH];
typedef uint8_t I2STxBuffer[I2S_TX_BUFFER_LENGTH];
/*----------------------------------------------------------------------------*/
/* Total: 13824 bytes */
[[gnu::section(".sram4")]] static I2SRxBuffer rxBuffersData[I2S_BUFFER_COUNT];
void *rxBuffers = rxBuffersData;
/*----------------------------------------------------------------------------*/
/* Total: 27648 bytes */
[[gnu::section(".sram2")]] static I2STxBuffer txBuffersData[I2S_BUFFER_COUNT];
void *txBuffers = txBuffersData;
/*----------------------------------------------------------------------------*/
/* Total: 16384 bytes */
[[gnu::section(".sram3")]] static FilePath trackBuffersData[TRACK_COUNT];
void *trackBuffers = trackBuffersData;

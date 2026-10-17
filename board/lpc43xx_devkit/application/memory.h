/*
 * board/lpc43xx_devkit/application/memory.h
 * Copyright (C) 2022 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef BOARD_LPC43XX_DEVKIT_APPLICATION_MEMORY_H_
#define BOARD_LPC43XX_DEVKIT_APPLICATION_MEMORY_H_
/*----------------------------------------------------------------------------*/
#include <stdint.h>
/*----------------------------------------------------------------------------*/
#define I2S_BUFFER_COUNT      3
#define I2S_RX_BUFFER_LENGTH  4608
#define I2S_TX_BUFFER_LENGTH  9216
#define TRACK_COUNT           256

/*
 * SD card read cache. The cache holds 16 KiB of data, split into
 * 512-byte sectors. Each cached sector occupies 512 bytes of data
 * plus a 16-byte block descriptor, so the arena is 32 * 528 bytes.
 */
#define SDMMC_SECTOR_SIZE   512
#define SDMMC_CACHE_SIZE    16384
#define SDMMC_CACHE_ARENA   16896

extern void *trackBuffers;
extern void *rxBuffers;
extern void *txBuffers;
extern void *sdmmcCache;
/*----------------------------------------------------------------------------*/
#endif /* BOARD_LPC43XX_DEVKIT_APPLICATION_MEMORY_H_ */

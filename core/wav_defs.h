/*
 * core/wav_defs.h
 * Copyright (C) 2022 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef CORE_WAV_DEFS_H_
#define CORE_WAV_DEFS_H_
/*----------------------------------------------------------------------------*/
#include <stdint.h>
/*----------------------------------------------------------------------------*/
struct [[gnu::packed]] WavHeader
{
  /* The "RIFF" chunk */
  uint32_t chunkId;
  uint32_t chunkSize;
  uint32_t format;

  /* The "fmt" subchunk */
  uint32_t subchunk1Id;
  uint32_t subchunk1Size;
  uint16_t audioFormat;
  uint16_t numChannels;
  uint32_t sampleRate;
  uint32_t byteRate;
  uint16_t blockAlign;
  uint16_t bitsPerSample;

  /* The "data" subchunk */
  uint32_t subchunk2Id;
  uint32_t subchunk2Size;
};
/*----------------------------------------------------------------------------*/
#endif /* CORE_WAV_DEFS_H_ */

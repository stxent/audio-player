/*
 * core/partitions.c
 * Copyright (C) 2022 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "partitions.h"
#include <xcore/memory.h>
/*----------------------------------------------------------------------------*/
#define SECTOR_SIZE 512
/*----------------------------------------------------------------------------*/
bool partitionReadHeader(struct Interface *interface, uint64_t offset,
    size_t index, struct PartitionDescriptor *desc)
{
  uint8_t buffer[SECTOR_SIZE];
  bool res = true;

  ifSetParam(interface, IF_ACQUIRE, 0);
  if (ifSetParam(interface, IF_POSITION_64, &offset) == E_OK)
  {
    if (ifRead(interface, buffer, sizeof(buffer)) != sizeof(buffer))
      res = false;
  }
  else
    res = false;
  ifSetParam(interface, IF_RELEASE, 0);

  if (!res)
    return false;

  if (fromBigEndian16(*(const uint16_t *)(buffer + 0x01FE)) != 0x55AA)
    return false;

  /* Pointer to a partition entry */
  const uint8_t * const entry = buffer + 0x01BE + (index << 4);

  /* Filter accepts inactive or bootable partition with non-zero type */
  if ((entry[0] == 0x00 || entry[0] >= 0x80) && entry[4] != 0x00)
  {
    desc->type = entry[4]; /* File system descriptor */
    desc->offset = (uint64_t)(*(const uint32_t *)(entry + 8)) * SECTOR_SIZE;
    desc->size = (uint64_t)(*(const uint32_t *)(entry + 12)) * SECTOR_SIZE;

    return true;
  }
  else
  {
    /* Empty entry */
    return false;
  }
}

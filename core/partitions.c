/*
 * core/partitions.c
 * Copyright (C) 2022 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "partitions.h"
#include <xcore/memory.h>
#include <string.h>
/*----------------------------------------------------------------------------*/
#define SECTOR_SIZE 512
/*----------------------------------------------------------------------------*/
bool partitionReadHeader(struct Interface *interface, uint64_t offset,
    size_t index, struct PartitionDescriptor *desc)
{
  uint8_t buffer[SECTOR_SIZE];
  bool res = true;

  ifSetParam(interface, IF_ACQUIRE, NULL);
  if (ifSetParam(interface, IF_POSITION_64, &offset) == E_OK)
  {
    if (ifRead(interface, buffer, sizeof(buffer)) != sizeof(buffer))
      res = false;
  }
  else
    res = false;
  ifSetParam(interface, IF_RELEASE, NULL);

  if (!res)
    return false;

  if (fromBigEndian16(*(const uint16_t *)(buffer + 0x01FE)) != 0x55AA)
    return false;

  /* Pointer to a partition entry */
  const uint8_t * const entry = buffer + 0x01BE + (index << 4);

  /* Filter accepts inactive or bootable partition with non-zero type */
  if ((entry[0] == 0x00 || entry[0] >= 0x80) && entry[4] != 0x00)
  {
    uint32_t tmp;

    desc->type = entry[4]; /* File system descriptor */
    memcpy(&tmp, entry + 8, sizeof(tmp));
    desc->offset = (uint64_t)tmp * SECTOR_SIZE;
    memcpy(&tmp, entry + 12, sizeof(tmp));
    desc->size = (uint64_t)tmp * SECTOR_SIZE;

    return true;
  }
  else
  {
    /* Empty entry */
    return false;
  }
}

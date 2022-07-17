/*
 * core/partitions.h
 * Copyright (C) 2022 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef CORE_PARTITIONS_H_
#define CORE_PARTITIONS_H_
/*----------------------------------------------------------------------------*/
#include <xcore/interface.h>
#include <stdbool.h>
#include <stdint.h>
/*----------------------------------------------------------------------------*/
struct PartitionDescriptor
{
  uint64_t offset;
  uint64_t size;
  uint8_t type;
};
/*----------------------------------------------------------------------------*/
BEGIN_DECLS

bool partitionReadHeader(struct Interface *, uint64_t, size_t,
    struct PartitionDescriptor *);

END_DECLS
/*----------------------------------------------------------------------------*/
#endif /* CORE_PARTITIONS_H_ */

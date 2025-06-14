/*
 * core/analog_filter.h
 * Copyright (C) 2022 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef CORE_ANALOG_FILTER_H_
#define CORE_ANALOG_FILTER_H_
/*----------------------------------------------------------------------------*/
#include <xcore/helpers.h>
#include <stddef.h>
#include <stdint.h>
/*----------------------------------------------------------------------------*/
#define ANALOG_FILTER_SIZE 11
/*----------------------------------------------------------------------------*/
struct AnalogFilter
{
  size_t index;
  size_t iteration;
  uint32_t seed;
  uint32_t sum;
  uint16_t buffer[ANALOG_FILTER_SIZE];
  bool first;
};
/*----------------------------------------------------------------------------*/
BEGIN_DECLS

void afAdd(struct AnalogFilter *, uint16_t);
void afInit(struct AnalogFilter *);
bool afSeedReady(const struct AnalogFilter *);
uint32_t afSeedValue(const struct AnalogFilter *);
uint16_t afValue(const struct AnalogFilter *);

END_DECLS
/*----------------------------------------------------------------------------*/
#endif /* CORE_ANALOG_FILTER_H_ */

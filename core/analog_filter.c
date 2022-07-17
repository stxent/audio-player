/*
 * core/analog_filter.c
 * Copyright (C) 2022 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "analog_filter.h"
#include <stdlib.h>
#include <string.h>
/*----------------------------------------------------------------------------*/
static int comparator(const void *a, const void *b)
{
  return (int)(*(const uint16_t *)a) - (int)(*(const uint16_t *)b);
}
/*----------------------------------------------------------------------------*/
void afAdd(struct AnalogFilter *filter, uint16_t value)
{
  if (filter->first)
  {
    filter->first = false;
    filter->sum = (uint32_t)value * ANALOG_FILTER_SIZE;

    for (size_t i = 0; i < ANALOG_FILTER_SIZE; ++i)
      filter->buffer[i] = value;
  }
  else
  {
    filter->sum -= filter->buffer[filter->index];
    filter->buffer[filter->index] = value;
    filter->sum += value;

    if (++filter->index == ANALOG_FILTER_SIZE)
      filter->index = 0;
  }
}
/*----------------------------------------------------------------------------*/
void afInit(struct AnalogFilter *filter)
{
  filter->first = true;
  filter->index = 0;
  filter->sum = 0;

  for (size_t i = 0; i < ANALOG_FILTER_SIZE; ++i)
    filter->buffer[i] = 0;
}
/*----------------------------------------------------------------------------*/
uint16_t afValue(const struct AnalogFilter *filter)
{
  uint16_t buffer[ANALOG_FILTER_SIZE];

  memcpy(buffer, filter->buffer, ANALOG_FILTER_SIZE * sizeof(uint16_t));
  qsort(buffer, ANALOG_FILTER_SIZE, sizeof(uint16_t), comparator);
  return buffer[ANALOG_FILTER_SIZE / 2];
}

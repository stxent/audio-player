/*
 * core/analog_filter.c
 * Copyright (C) 2022 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "analog_filter.h"
#include <string.h>
/*----------------------------------------------------------------------------*/
static uint16_t findSmallestElement(uint16_t *input, size_t count)
{
  const size_t k = count / 2;
  size_t l = 0;
  size_t m = count - 1;

  while (l < m)
  {
    const uint16_t value = input[k];
    size_t i = l;
    size_t j = m;

    do
    {
      while (input[i] < value)
        i++;
      while (value < input[j])
        j--;

      if (i <= j)
      {
        const uint16_t swap = input[i];
        input[i] = input[j];
        input[j] = swap;

        i++;
        j--;
      }
    }
    while (i <= j);

    if (j < k)
      l = i;
    if (k < i)
      m = j;
  }

  return input[k];
}
/*----------------------------------------------------------------------------*/
void afAdd(struct AnalogFilter *filter, uint16_t value)
{
  if (filter->iteration)
  {
    filter->seed <<= 1;
    filter->seed |= value & 1;
    --filter->iteration;
  }

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
  filter->iteration = sizeof(filter->seed) * 8;
  filter->seed = 0;
  filter->sum = 0;

  for (size_t i = 0; i < ANALOG_FILTER_SIZE; ++i)
    filter->buffer[i] = 0;
}
/*----------------------------------------------------------------------------*/
bool afSeedReady(const struct AnalogFilter *filter)
{
  return filter->iteration == 0;
}
/*----------------------------------------------------------------------------*/
uint32_t afSeedValue(const struct AnalogFilter *filter)
{
  return filter->seed;
}
/*----------------------------------------------------------------------------*/
uint16_t afValue(const struct AnalogFilter *filter)
{
  uint16_t buffer[ANALOG_FILTER_SIZE];

  memcpy(buffer, filter->buffer, sizeof(buffer));
  return findSmallestElement(buffer, ANALOG_FILTER_SIZE);
}

/*
 * amplifier.c
 * Copyright (C) 2023 xent
 * Project is distributed under the terms of the MIT License
 */

#include "amplifier.h"
#include "amplifier_defs.h"
#include <halm/generic/i2c.h>
#include <halm/generic/work_queue.h>
#include <halm/timer.h>
#include <assert.h>
/*----------------------------------------------------------------------------*/
enum State
{
  STATE_IDLE,

  STATE_CONFIG_WAIT,
  STATE_CONFIG_START,
  STATE_CONFIG_BUS_WAIT,

  STATE_ERROR_WAIT,
  STATE_ERROR_INTERFACE,
  STATE_ERROR_TIMEOUT
} __attribute__((packed));
/*----------------------------------------------------------------------------*/
static void busInit(struct Amplifier *, bool);
static void invokeUpdate(struct Amplifier *);
static void onBusEvent(void *);
static void onTimerEvent(void *);
static void startBusTimeout(struct Timer *);
static void updateTask(void *);
/*----------------------------------------------------------------------------*/
static enum Result ampInit(void *, const void *);
static void ampDeinit(void *);
/*----------------------------------------------------------------------------*/
const struct EntityClass * const Amplifier = &(const struct EntityClass){
    .size = sizeof(struct Amplifier),
    .init = ampInit,
    .deinit = ampDeinit
};
/*----------------------------------------------------------------------------*/
static void busInit(struct Amplifier *amp, bool read)
{
  /* Lock the interface */
  ifSetParam(amp->bus, IF_ACQUIRE, NULL);

  ifSetParam(amp->bus, IF_ADDRESS, &amp->address);
  ifSetParam(amp->bus, IF_ZEROCOPY, NULL);
  ifSetCallback(amp->bus, onBusEvent, amp);

  if (amp->rate)
    ifSetParam(amp->bus, IF_RATE, &amp->rate);

  if (read)
    ifSetParam(amp->bus, IF_I2C_REPEATED_START, NULL);

  /* Start bus watchdog */
  startBusTimeout(amp->timer);
}
/*----------------------------------------------------------------------------*/
static void invokeUpdate(struct Amplifier *amp)
{
  assert(amp->updateCallback != NULL || amp->wq != NULL);

  if (amp->updateCallback != NULL)
  {
    amp->updateCallback(amp->updateCallbackArgument);
  }
  else if (!amp->pending)
  {
    amp->pending = true;

    if (wqAdd(amp->wq, updateTask, amp) != E_OK)
      amp->pending = false;
  }
}
/*----------------------------------------------------------------------------*/
static void onBusEvent(void *object)
{
  struct Amplifier * const amp = object;

  timerDisable(amp->timer);

  if (ifGetParam(amp->bus, IF_STATUS, NULL) != E_OK)
  {
    amp->transfer.state = STATE_ERROR_WAIT;

    /* Start bus timeout sequence */
    startBusTimeout(amp->timer);
  }
  else
  {
    amp->ready = true;
    amp->transfer.state = STATE_IDLE;
  }

  ifSetCallback(amp->bus, NULL, NULL);
  ifSetParam(amp->bus, IF_RELEASE, NULL);
  invokeUpdate(amp);
}
/*----------------------------------------------------------------------------*/
static void onTimerEvent(void *object)
{
  struct Amplifier * const amp = object;

  switch (amp->transfer.state)
  {
    case STATE_CONFIG_WAIT:
      amp->transfer.state = STATE_CONFIG_START;
      break;

    case STATE_ERROR_WAIT:
      amp->transfer.state = STATE_ERROR_INTERFACE;
      break;

    default:
      ifSetCallback(amp->bus, NULL, NULL);
      ifSetParam(amp->bus, IF_RELEASE, NULL);
      amp->transfer.state = STATE_ERROR_TIMEOUT;
      break;
  }

  invokeUpdate(amp);
}
/*----------------------------------------------------------------------------*/
static void startBusTimeout(struct Timer *timer)
{
  timerSetOverflow(timer, timerGetFrequency(timer) / 10);
  timerSetValue(timer, 0);
  timerEnable(timer);
}
/*----------------------------------------------------------------------------*/
static void updateTask(void *argument)
{
  struct Amplifier * const amp = argument;

  amp->pending = false;
  ampUpdate(amp);
}
/*----------------------------------------------------------------------------*/
static enum Result ampInit(void *object, const void *arguments)
{
  const struct AmplifierConfig * const config = arguments;
  assert(config != NULL);
  assert(config->bus != NULL && config->timer != NULL);

  struct Amplifier * const amp = object;

  amp->bus = config->bus;
  amp->timer = config->timer;
  amp->wq = NULL;

  amp->errorCallback = NULL;
  amp->errorCallbackArgument = NULL;
  amp->updateCallback = NULL;
  amp->updateCallbackArgument = NULL;

  amp->address = config->address;
  amp->rate = config->rate;
  amp->config = 0;
  amp->changed = false;
  amp->pending = false;
  amp->ready = false;

  amp->transfer.state = STATE_IDLE;

  timerSetAutostop(amp->timer, true);
  timerSetCallback(amp->timer, onTimerEvent, amp);

  return E_OK;
}
/*----------------------------------------------------------------------------*/
static void ampDeinit(void *object)
{
  struct Amplifier * const amp = object;

  timerDisable(amp->timer);
  timerSetCallback(amp->timer, NULL, NULL);
}
/*----------------------------------------------------------------------------*/
void ampSetErrorCallback(void *object, void (*callback)(void *),
    void *argument)
{
  struct Amplifier * const amp = object;

  assert(callback != NULL);

  amp->errorCallbackArgument = argument;
  amp->errorCallback = callback;
}
/*----------------------------------------------------------------------------*/
void ampSetUpdateCallback(void *object, void (*callback)(void *),
    void *argument)
{
  struct Amplifier * const amp = object;

  assert(callback != NULL);
  assert(amp->wq == NULL);

  amp->updateCallbackArgument = argument;
  amp->updateCallback = callback;
}
/*----------------------------------------------------------------------------*/
void ampSetUpdateWorkQueue(void *object, struct WorkQueue *wq)
{
  struct Amplifier * const amp = object;

  assert(wq != NULL);
  assert(amp->updateCallback == NULL);

  amp->wq = wq;
}
/*----------------------------------------------------------------------------*/
bool ampIsReady(const void *object)
{
  const struct Amplifier * const amp = object;
  return amp->ready;
}
/*----------------------------------------------------------------------------*/
void ampReset(void *object, uint8_t gain, bool enabled)
{
  struct Amplifier * const amp = object;
  uint8_t config = 0;

  if (gain & 0x01)
    config |= AMP_CTL_GAIN0;
  if (gain & 0x02)
    config |= AMP_CTL_GAIN1;
  if (enabled)
    config |= AMP_CTL_POWER;

  amp->config = config;
  amp->changed = true;

  invokeUpdate(amp);
}
/*----------------------------------------------------------------------------*/
bool ampUpdate(void *object)
{
  struct Amplifier * const amp = object;
  bool busy;
  bool updated;

  do
  {
    busy = false;
    updated = false;

    switch (amp->transfer.state)
    {
      case STATE_IDLE:
        if (amp->changed)
        {
          amp->transfer.state = STATE_CONFIG_WAIT;
          startBusTimeout(amp->timer);
        }
        break;

      case STATE_CONFIG_START:
        busy = true;

        amp->changed = false;
        amp->transfer.state = STATE_CONFIG_BUS_WAIT;

        amp->transfer.buffer[0] = AMP_REG_CTL;
        amp->transfer.buffer[1] = amp->config;

        busInit(amp, false);
        ifWrite(amp->bus, amp->transfer.buffer, sizeof(amp->transfer.buffer));
        break;

      case STATE_ERROR_INTERFACE:
      case STATE_ERROR_TIMEOUT:
        amp->ready = false;
        amp->transfer.state = STATE_IDLE;

        if (amp->errorCallback != NULL)
          amp->errorCallback(amp->errorCallbackArgument);

        updated = true;
        break;

      default:
        break;
    }
  }
  while (updated);

  return busy;
}

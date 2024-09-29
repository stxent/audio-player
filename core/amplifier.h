/*
 * core/amplifier.h
 * Copyright (C) 2023 xent
 * Project is distributed under the terms of the MIT License
 */

#ifndef CORE_AMPLIFIER_H_
#define CORE_AMPLIFIER_H_
/*----------------------------------------------------------------------------*/
#include <xcore/entity.h>
#include <stdbool.h>
#include <stdint.h>
/*----------------------------------------------------------------------------*/
#define AMP_ADDRESS   0x15
#define AMP_GAIN_MIN  0
#define AMP_GAIN_MAX  3
/*----------------------------------------------------------------------------*/
extern const struct EntityClass * const Amplifier;

struct Interface;
struct Timer;

struct AmplifierConfig
{
  /** Mandatory: management interface. */
  struct Interface *bus;
  /** Mandatory: timer instance for delays and watchdogs. */
  struct Timer *timer;
  /** Optional: amplifier address. */
  uint32_t address;
  /** Optional: amplifier management interface rate. */
  uint32_t rate;
};

struct Amplifier
{
  struct Entity base;

  void (*errorCallback)(void *);
  void *errorCallbackArgument;
  void (*idleCallback)(void *);
  void *idleCallbackArgument;
  void (*updateCallback)(void *);
  void *updateCallbackArgument;

  struct Interface *bus;
  struct Timer *timer;
  struct WorkQueue *wq;

  uint32_t address;
  uint32_t rate;
  uint8_t config;
  uint8_t leds;
  bool changed;
  bool pending;
  bool ready;

  struct
  {
    uint8_t buffer[3];
    uint8_t state;
  } transfer;
};
/*----------------------------------------------------------------------------*/
BEGIN_DECLS

void ampSetErrorCallback(void *, void (*)(void *), void *);
void ampSetIdleCallback(void *, void (*)(void *), void *);
void ampSetUpdateCallback(void *, void (*)(void *), void *);
void ampSetUpdateWorkQueue(void *, struct WorkQueue *);

bool ampIsReady(const void *);
void ampReset(void *, uint8_t, bool);
void ampSetDebugValue(void *, uint8_t);
bool ampUpdate(void *);

END_DECLS
/*----------------------------------------------------------------------------*/
#endif /* CORE_AMPLIFIER_H_ */

/*
 * board/lpc43xx_devkit/shared/board_shared.h
 * Copyright (C) 2022 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef BOARD_LPC43XX_DEVKIT_SHARED_BOARD_SHARED_H_
#define BOARD_LPC43XX_DEVKIT_SHARED_BOARD_SHARED_H_
/*----------------------------------------------------------------------------*/
#include "analog_filter.h"
#include <dpm/bus_handler.h>
#include <halm/generic/work_queue_irq.h>
#include <halm/pin.h>
#include <xcore/helpers.h>
#include <stdbool.h>
#include <stdint.h>
/*----------------------------------------------------------------------------*/
#define BOARD_ADC_PIN         PIN(PORT_ADC, 0)
#define BOARD_BUTTON_1_PIN    PIN(PORT_7, 7)  /* Previous */
#define BOARD_BUTTON_2_PIN    PIN(PORT_7, 5)  /* Stop */
#define BOARD_BUTTON_3_PIN    PIN(PORT_7, 0)  /* Play/Pause */
#define BOARD_BUTTON_4_PIN    PIN(PORT_2, 13) /* Next */
#define BOARD_CODEC_RESET_PIN PIN(PORT_4, 1)
#define BOARD_LED_R_PIN       PIN(PORT_5, 7)
#define BOARD_LED_G_PIN       PIN(PORT_5, 5)
#define BOARD_LED_B_PIN       PIN(PORT_4, 0)
#define BOARD_LED_WA_PIN      PIN(PORT_6, 8)
#define BOARD_LED_WB_PIN      PIN(PORT_6, 7)
#define BOARD_POWER_PIN       PIN(PORT_1, 8)
/*----------------------------------------------------------------------------*/
DEFINE_WQ_IRQ(WQ_LP)

struct Entity;
struct GpioBus;
struct Interface;
struct Timer;
struct TimerFactory;
struct Watchdog;

struct AnalogPackage
{
  struct Interface *adc;
  struct Timer *timer;
  uint8_t value;
};

struct ButtonPackage
{
  struct GpioBus *buttons;
  struct Timer *timer;
  uint8_t debounce[4];
};

struct ChronoPackage
{
  struct Timer *timer;
  struct TimerFactory *factory;

  struct Timer *guardTimer;
};

struct CodecPackage
{
  struct Entity *amp;
  struct Timer *ampTimer;
  struct Entity *codec;
  struct Timer *codecTimer;
  struct Interface *i2c;

  struct BusHandler handler;
};
/*----------------------------------------------------------------------------*/
BEGIN_DECLS

struct Entity *boardMakeAmp(struct Interface *, struct Timer *);
struct Entity *boardMakeCodec(struct Interface *, struct Timer *);
struct Timer *boardMakeCodecTimer(void);
struct Timer *boardMakeLoadTimer(void);
struct Timer *boardMakeMountTimer(void);
struct Interface *boardMakeI2C(void);
struct Interface *boardMakeI2S(void);
struct Interface *boardMakeSDMMC(void);
struct Interface *boardMakeSerial(void);
struct Watchdog *boardMakeWatchdog(void);

bool boardSetupAnalogPackage(struct AnalogPackage *);
bool boardSetupButtonPackage(struct ButtonPackage *);
bool boardSetupChronoPackage(struct ChronoPackage *);
bool boardSetupCodecPackage(struct CodecPackage *, struct TimerFactory *);
bool boardSetupClock(void);

END_DECLS
/*----------------------------------------------------------------------------*/
#endif /* BOARD_LPC43XX_DEVKIT_SHARED_BOARD_SHARED_H_ */

/*
 * board/lpc17xx_devkit/shared/board_shared.h
 * Copyright (C) 2022 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef BOARD_LPC17XX_DEVKIT_SHARED_BOARD_SHARED_H_
#define BOARD_LPC17XX_DEVKIT_SHARED_BOARD_SHARED_H_
/*----------------------------------------------------------------------------*/
#include "analog_filter.h"
#include <dpm/bus_handler.h>
#include <halm/generic/work_queue_irq.h>
#include <halm/pin.h>
#include <xcore/helpers.h>
#include <stdbool.h>
#include <stdint.h>
/*----------------------------------------------------------------------------*/
/* #define AUDIOBOX_V1 */

#ifdef AUDIOBOX_V1
#  define BOARD_ADC_PIN         PIN(0, 2)
#  define BOARD_BUTTON_1_PIN    PIN(0, 15) /* Previous */
#  define BOARD_BUTTON_2_PIN    PIN(4, 28) /* Stop */
#  define BOARD_BUTTON_3_PIN    PIN(0, 16) /* Play/Pause */
#  define BOARD_BUTTON_4_PIN    PIN(4, 29) /* Next */
#  define BOARD_CODEC_CLOCK_PIN 0
#  define BOARD_CODEC_RESET_PIN PIN(0, 3)
#  define BOARD_LED_R_PIN       PIN(1, 18)
#  define BOARD_LED_G_PIN       PIN(1, 23)
#  define BOARD_LED_B_PIN       PIN(1, 24)
#  define BOARD_LED_IND_A_PIN   PIN(1, 8)
#  define BOARD_LED_IND_B_PIN   PIN(1, 9)
#  define BOARD_POWER_PIN       PIN(2, 8)
#  define BOARD_SDIO_CS_PIN     PIN(0, 22)
#else
#  define BOARD_ADC_PIN         PIN(0, 2)
#  define BOARD_BUTTON_1_PIN    PIN(1, 18) /* Previous */
#  define BOARD_BUTTON_2_PIN    PIN(1, 23) /* Stop */
#  define BOARD_BUTTON_3_PIN    PIN(1, 24) /* Play/Pause */
#  define BOARD_BUTTON_4_PIN    PIN(1, 26) /* Next */
#  define BOARD_CODEC_CLOCK_PIN PIN(4, 29)
#  define BOARD_CODEC_RESET_PIN PIN(0, 3)
#  define BOARD_LED_R_PIN       PIN(1, 10)
#  define BOARD_LED_G_PIN       PIN(1, 9)
#  define BOARD_LED_B_PIN       PIN(1, 8)
#  define BOARD_LED_IND_A_PIN   PIN(2, 0)
#  define BOARD_LED_IND_B_PIN   PIN(2, 1)
#  define BOARD_POWER_PIN       PIN(2, 8)
#  define BOARD_SDIO_CS_PIN     PIN(0, 22)
#endif
/*----------------------------------------------------------------------------*/
struct Entity;
struct GpioBus;
struct Interface;
struct Timer;
struct Watchdog;

DEFINE_WQ_IRQ(WQ_LP)

struct AnalogPackage
{
  struct Interface *adc;
  struct Timer *timer;

  struct AnalogFilter filter;
  uint8_t value;
};

struct ButtonPackage
{
  struct GpioBus *buttons;
  struct Timer *timer;
  uint8_t debounce[4];
};

struct CodecPackage
{
  struct SoftwareTimerFactory *factory;
  struct Timer *baseTimer;

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
struct Timer *boardMakeMemoryTimer(void);
struct Timer *boardMakeMountTimer(void);
struct Interface *boardMakeI2C(void);
struct Interface *boardMakeI2S(void);
struct Interface *boardMakeSDIO(struct Interface *, struct Timer *);
struct Interface *boardMakeSerial(void);
struct Interface *boardMakeSPI(void);
struct Watchdog *boardMakeWatchdog(void);

bool boardSetupAnalogPackage(struct AnalogPackage *);
bool boardSetupButtonPackage(struct ButtonPackage *);
bool boardSetupCodecPackage(struct CodecPackage *);
bool boardSetupClock(void);

END_DECLS
/*----------------------------------------------------------------------------*/
#endif /* BOARD_LPC17XX_DEVKIT_SHARED_BOARD_SHARED_H_ */

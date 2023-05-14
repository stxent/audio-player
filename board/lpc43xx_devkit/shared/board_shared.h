/*
 * board/lpc43xx_devkit/shared/board_shared.h
 * Copyright (C) 2022 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef BOARD_LPC43XX_DEVKIT_SHARED_BOARD_SHARED_H_
#define BOARD_LPC43XX_DEVKIT_SHARED_BOARD_SHARED_H_
/*----------------------------------------------------------------------------*/
#include "analog_filter.h"
#include <halm/generic/work_queue_irq.h>
#include <halm/pin.h>
#include <xcore/helpers.h>
#include <stdbool.h>
#include <stdint.h>
/*----------------------------------------------------------------------------*/
struct Entity;
struct GpioBus;
struct Interface;
struct Timer;
struct Watchdog;
/*----------------------------------------------------------------------------*/
#define BOARD_BUTTON_1_PIN    PIN(PORT_B, 5)
#define BOARD_BUTTON_2_PIN    PIN(PORT_3, 5)
#define BOARD_BUTTON_3_PIN    PIN(PORT_B, 3)
#define BOARD_BUTTON_4_PIN    PIN(PORT_7, 3)
#define BOARD_CODEC_RESET_PIN PIN(PORT_4, 1)
#define BOARD_LED_R_PIN       PIN(PORT_7, 7)
#define BOARD_LED_G_PIN       PIN(PORT_C, 11)
#define BOARD_LED_B_PIN       PIN(PORT_9, 3)
#define BOARD_LED_W_PIN       PIN(PORT_9, 4)

DEFINE_WQ_IRQ(WQ_LP)

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
/*----------------------------------------------------------------------------*/
BEGIN_DECLS

struct Entity *boardMakeCodec(struct Interface *, struct Timer *timer);
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
bool boardSetupClock(void);

void codecSetRate(struct Entity *, uint32_t);
void codecSetVolume(struct Entity *, uint8_t);

END_DECLS
/*----------------------------------------------------------------------------*/
#endif /* BOARD_LPC43XX_DEVKIT_SHARED_BOARD_SHARED_H_ */

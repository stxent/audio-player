/*
 * board/lpc17xx_devkit/shared/board_shared.h
 * Copyright (C) 2022 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef BOARD_LPC17XX_DEVKIT_SHARED_BOARD_SHARED_H_
#define BOARD_LPC17XX_DEVKIT_SHARED_BOARD_SHARED_H_
/*----------------------------------------------------------------------------*/
#include "analog_filter.h"
#include <xcore/helpers.h>
#include <stdbool.h>
#include <stdint.h>
/*----------------------------------------------------------------------------*/
struct Entity;
struct GpioBus;
struct Interface;
struct Timer;
/*----------------------------------------------------------------------------*/
#define BOARD_BUTTON_PIN    PIN(2, 10)
#define BOARD_BUTTON_1_PIN  PIN(1, 18)
#define BOARD_BUTTON_2_PIN  PIN(1, 23)
#define BOARD_BUTTON_3_PIN  PIN(1, 24)
#define BOARD_BUTTON_4_PIN  PIN(1, 26)
#define BOARD_LED_R_PIN     PIN(1, 10)
#define BOARD_LED_G_PIN     PIN(1, 9)
#define BOARD_LED_B_PIN     PIN(1, 8)
#define BOARD_SDIO_CS_PIN   PIN(0, 22)

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
/*----------------------------------------------------------------------------*/
BEGIN_DECLS

struct Entity *boardMakeCodec(struct Interface *);
struct Timer *boardMakeLoadTimer(void);
struct Timer *boardMakeMemoryTimer(void);
struct Timer *boardMakeMountTimer(void);
struct Interface *boardMakeI2C(void);
struct Interface *boardMakeI2S(void);
struct Interface *boardMakeSDIO(struct Interface *, struct Timer *);
struct Interface *boardMakeSerial(void);
struct Interface *boardMakeSPI(void);
// struct Interface *boardMakeUSB(void); // TODO Package

bool boardSetupAnalogPackage(struct AnalogPackage *);
bool boardSetupButtonPackage(struct ButtonPackage *);
bool boardSetupClock(void);

void codecSetRate(struct Entity *, uint32_t);
void codecSetVolume(struct Entity *, uint8_t);

END_DECLS
/*----------------------------------------------------------------------------*/
#endif /* BOARD_LPC17XX_DEVKIT_SHARED_BOARD_SHARED_H_ */

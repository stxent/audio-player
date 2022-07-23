/*
 * board/lpc43xx_devkit/shared/board_shared.c
 * Copyright (C) 2022 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "board_shared.h"
#include "tlv320aic3x.h"
#include <dpm/button.h>
#include <halm/core/cortex/systick.h>
#include <halm/gpio_bus.h>
#include <halm/platform/lpc/adc_dma.h>
#include <halm/platform/lpc/clocking.h>
#include <halm/platform/lpc/gptimer.h>
#include <halm/platform/lpc/i2c.h>
#include <halm/platform/lpc/i2s_dma.h>
#include <halm/platform/lpc/rit.h>
#include <halm/platform/lpc/sdmmc.h>
#include <halm/platform/lpc/serial.h>
#include <string.h>
/*----------------------------------------------------------------------------*/
static const PinNumber adcPinArray[] = {
    PIN(PORT_ADC, 5),
    0
};

static const struct AdcDmaConfig adcConfig = {
    .pins = adcPinArray,
    .event = ADC_CTOUT_15,
    .channel = 0,
    .dma = 4
};

static const struct SimpleGpioBusConfig busConfig = {
    .pins = (const PinNumber []){
        BOARD_BUTTON_1_PIN,
        BOARD_BUTTON_2_PIN,
        BOARD_BUTTON_3_PIN,
        BOARD_BUTTON_4_PIN,
        0
    },
    .direction = PIN_INPUT,
    .pull = PIN_PULLUP
 };

static const struct GpTimerConfig adcTimerConfig = {
    .frequency = 1000000,
    .event = GPTIMER_MATCH3,
    .channel = 3
};

static const struct GpTimerConfig loadTimerConfig = {
    .frequency = 1000000,
    .priority = 1,
    .channel = 1
};

static const struct I2CConfig i2cConfig = {
    .rate = 400000, /* Initial rate */
    .scl = PIN(PORT_I2C, PIN_I2C0_SCL),
    .sda = PIN(PORT_I2C, PIN_I2C0_SDA),
    .priority = 0,
    .channel = 0
};

static const struct I2SDmaConfig i2sConfig = {
    .size = 2,
    .rate = 44100,
    .width = I2S_WIDTH_16,
    .tx = {
        .sda = PIN(PORT_3, 2),
        .sck = PIN(PORT_3, 0),
        .ws = PIN(PORT_3, 1),
        .dma = 0
    },
    .rx = {
        .sda = PIN(PORT_6, 2),
        .dma = 1
    },
    .priority = 0,
    .channel = 0,
    .mono = false,
    .slave = false
};

static const struct SerialConfig serialConfig = {
    .rxLength = 16,
    .txLength = 128,
    .rate = 19200,
    .rx = PIN(PORT_F, 11),
    .tx = PIN(PORT_F, 10),
    .channel = 0
};

static const struct SdmmcConfig sdmmcConfig = {
    .rate = 12000000,
    .clk = PIN(PORT_CLK, 2),
    .cmd = PIN(PORT_C, 10),
    .dat0 = PIN(PORT_C, 4)
};
/*----------------------------------------------------------------------------*/
static const struct GenericClockConfig initialClockConfig = {
    .source = CLOCK_INTERNAL
};

static const struct GenericClockConfig mainClockConfig = {
    .source = CLOCK_PLL
};

static const struct GenericClockConfig sdClockConfig = {
    .source = CLOCK_IDIVB
};

static const struct GenericDividerConfig dividerConfig = {
    .source = CLOCK_PLL,
    .divisor = 1
};

static const struct ExternalOscConfig extOscConfig = {
    .frequency = 12000000,
    .bypass = false
};

static const struct PllConfig sysPllConfig = {
    .source = CLOCK_EXTERNAL,
    .divisor = 4,
    .multiplier = 17
};
/*----------------------------------------------------------------------------*/
struct Entity *boardMakeCodec(struct Interface *i2c)
{
  const struct TLV320AIC3xConfig codecConfig = {
      .interface = i2c,
      .address = 0,
      .rate = 0,
      .reset = 0
  };
  struct TLV320AIC3x * const codec = init(TLV320AIC3x, &codecConfig);

  if (codec)
  {
    aic3xEnablePath(codec, AIC3X_LINE_IN);
    aic3xEnablePath(codec, AIC3X_LINE_OUT);
  }

  return (struct Entity *)codec;
}
/*----------------------------------------------------------------------------*/
struct Timer *boardMakeLoadTimer(void)
{
  return init(GpTimer, &loadTimerConfig);
}
/*----------------------------------------------------------------------------*/
struct Timer *boardMakeMountTimer(void)
{
  return init(Rit, 0);
}
/*----------------------------------------------------------------------------*/
struct Interface *boardMakeI2C(void)
{
  return init(I2C, &i2cConfig);
}
/*----------------------------------------------------------------------------*/
struct Interface *boardMakeI2S(void)
{
  return init(I2SDma, &i2sConfig);
}
/*----------------------------------------------------------------------------*/
struct Interface *boardMakeSDMMC(void)
{
  return init(Sdmmc, &sdmmcConfig);
}
/*----------------------------------------------------------------------------*/
struct Interface *boardMakeSerial(void)
{
  return init(Serial, &serialConfig);
}
/*----------------------------------------------------------------------------*/
bool boardSetupAnalogPackage(struct AnalogPackage *package)
{
  package->timer = 0;

  package->adc = init(AdcDma, &adcConfig);
  if (!package->adc)
    return false;

  /*
  * The overflow frequency of the timer should be two times higher
  * than that of the hardware events for ADC.
  */
  package->timer = init(GpTimer, &adcTimerConfig);
  if (!package->timer)
    return false;

  package->value = 0;
  return true;
}
/*----------------------------------------------------------------------------*/
bool boardSetupButtonPackage(struct ButtonPackage *package)
{
  package->timer = 0;

  package->buttons = init(SimpleGpioBus, &busConfig);
  if (!package->buttons)
    return false;

  package->timer = init(SysTickTimer, 0);
  if (!package->timer)
    return false;

  memset(package->debounce, 0, sizeof(package->debounce));
  return true;
}
/*----------------------------------------------------------------------------*/
bool boardSetupClock(void)
{
  clockEnable(MainClock, &initialClockConfig);

  clockEnable(ExternalOsc, &extOscConfig);
  while (!clockReady(ExternalOsc));

  clockEnable(SystemPll, &sysPllConfig);
  while (!clockReady(SystemPll));

  clockEnable(DividerB, &dividerConfig);
  while (!clockReady(DividerB));

  clockEnable(SdioClock, &sdClockConfig);
  while (!clockReady(SdioClock));

  /* I2S, I2C */
  clockEnable(Apb1Clock, &mainClockConfig);
  while (!clockReady(Apb1Clock));

  /* ADC0 */
  clockEnable(Apb3Clock, &mainClockConfig);
  while (!clockReady(Apb3Clock));

  clockEnable(Usart0Clock, &mainClockConfig);
  while (!clockReady(Usart0Clock));

  clockEnable(MainClock, &mainClockConfig);

  return true;
}
/*----------------------------------------------------------------------------*/
void codecSetRate(struct Entity *codec, uint32_t value)
{
  // TODO
  (void)codec;
  (void)value;
}
/*----------------------------------------------------------------------------*/
void codecSetVolume(struct Entity *codec, uint8_t value)
{
  int gain;

  if (value > 100)
    value = 100;

  if (value > 0)
    gain = ((100 - value) * 60) / 100;
  else
    gain = 127; /* Mute */

  aic3xSetGain((struct TLV320AIC3x *)codec, AIC3X_LINE_OUT, gain);
}

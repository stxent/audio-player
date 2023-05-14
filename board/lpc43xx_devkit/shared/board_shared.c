/*
 * board/lpc43xx_devkit/shared/board_shared.c
 * Copyright (C) 2022 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "board_shared.h"
#include <dpm/audio/tlv320aic3x.h>
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
#include <halm/platform/lpc/wdt.h>
#include <string.h>
/*----------------------------------------------------------------------------*/
#define PRI_TIMER_DBG 2

#define PRI_TIMER_I2C 1
#define PRI_I2C       1
#define PRI_I2S       1
#define PRI_SDMMC     1
#define PRI_SERIAL    1
/* GPDMA 1 */

#define PRI_TIMER_SYS 0
/*----------------------------------------------------------------------------*/
static const PinNumber adcPinArray[] = {
    PIN(PORT_ADC, 5),
    0
};

static const struct AdcDmaConfig adcConfig = {
    .pins = adcPinArray,
    .event = ADC_CTOUT_15,
    .channel = 0,
    .dma = 2
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

static const struct GpTimerConfig codecTimerConfig = {
    .frequency = 1000000,
    .priority = PRI_TIMER_I2C,
    .channel = 2
};

static const struct GpTimerConfig loadTimerConfig = {
    .frequency = 1000000,
    .priority = PRI_TIMER_DBG,
    .channel = 1
};

static const struct I2CConfig i2cConfig = {
    .rate = 400000, /* Initial rate */
    .scl = PIN(PORT_I2C, PIN_I2C0_SCL),
    .sda = PIN(PORT_I2C, PIN_I2C0_SDA),
    .priority = PRI_I2C,
    .channel = 0
};

static const struct I2SDmaConfig i2sConfig = {
    .size = 2,
    .rate = 44100,
    .width = I2S_WIDTH_16,
    .tx = {
        .sda = PIN(PORT_7, 2),
        .sck = PIN(PORT_4, 7),
        .ws = PIN(PORT_7, 1),
        .dma = 0
    },
    .rx = {
        .sda = PIN(PORT_6, 2),
        .dma = 1
    },
    .priority = PRI_I2S,
    .channel = 0,
    .mono = false,
    .slave = false
};

static const struct SerialConfig serialConfig = {
    .rxLength = 16,
    .txLength = 128,
    .rate = 115200,
    .rx = PIN(PORT_1, 14),
    .tx = PIN(PORT_5, 6),
    .priority = PRI_SERIAL,
    .channel = 1
};

static const struct SdmmcConfig sdmmcConfig = {
    .rate = 17000000,
    .clk = PIN(PORT_CLK, 0),
    .cmd = PIN(PORT_1, 6),
    .dat0 = PIN(PORT_1, 9),
    .dat1 = PIN(PORT_1, 10),
    .dat2 = PIN(PORT_1, 11),
    .dat3 = PIN(PORT_1, 12),
    .priority = PRI_SDMMC
};

static const struct WdtConfig wdtConfig = {
    .period = 2000
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
struct Entity *boardMakeCodec(struct Interface *i2c, struct Timer *timer)
{
  const struct TLV320AIC3xConfig codecConfig = {
      .bus = i2c,
      .timer = timer,
      .address = 0,
      .rate = 0,
      .reset = BOARD_CODEC_RESET_PIN
  };
  struct TLV320AIC3x * const codec = init(TLV320AIC3x, &codecConfig);

  if (codec != NULL)
  {
    aic3xSetUpdateWorkQueue(codec, WQ_LP);
    aic3xReset(codec, 44100, AIC3X_NONE, 0, false, AIC3X_LINE_OUT_DIFF, 0);
  }

  return (struct Entity *)codec;
}
/*----------------------------------------------------------------------------*/
struct Timer *boardMakeCodecTimer(void)
{
  return init(GpTimer, &codecTimerConfig);
}
/*----------------------------------------------------------------------------*/
struct Timer *boardMakeLoadTimer(void)
{
  return init(GpTimer, &loadTimerConfig);
}
/*----------------------------------------------------------------------------*/
struct Timer *boardMakeMountTimer(void)
{
  return init(Rit, &(struct RitConfig){PRI_TIMER_SYS});
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
struct Watchdog *boardMakeWatchdog(void)
{
  return init(Wdt, &wdtConfig);
}
/*----------------------------------------------------------------------------*/
bool boardSetupAnalogPackage(struct AnalogPackage *package)
{
  package->timer = NULL;

  package->adc = init(AdcDma, &adcConfig);
  if (package->adc == NULL)
    return false;

  /*
  * The overflow frequency of the timer should be two times higher
  * than that of the hardware events for ADC.
  */
  package->timer = init(GpTimer, &adcTimerConfig);
  if (package->timer == NULL)
    return false;

  package->value = 0;
  return true;
}
/*----------------------------------------------------------------------------*/
bool boardSetupButtonPackage(struct ButtonPackage *package)
{
  package->timer = NULL;

  package->buttons = init(SimpleGpioBus, &busConfig);
  if (package->buttons == NULL)
    return false;

  package->timer = init(SysTick, &(struct SysTickConfig){PRI_TIMER_SYS});
  if (package->timer == NULL)
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
  aic3xSetRate((struct TLV320AIC3x *)codec, value);
}
/*----------------------------------------------------------------------------*/
void codecSetVolume(struct Entity *codec, uint8_t value)
{
  aic3xSetOutputGain((struct TLV320AIC3x *)codec, value);
}

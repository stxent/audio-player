/*
 * board/lpc43xx_devkit/shared/board_shared.c
 * Copyright (C) 2022 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "amplifier.h"
#include "board_shared.h"
#include <dpm/audio/tlv320aic3x.h>
#include <dpm/bus_handler.h>
#include <dpm/button.h>
#include <halm/core/cortex/systick.h>
#include <halm/generic/timer_factory.h>
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
/* WQ_LP 0 */
/*----------------------------------------------------------------------------*/
static struct ClockSettings sharedClockSettings
    __attribute__((section(".shared")));
/*----------------------------------------------------------------------------*/
struct Entity *boardMakeAmp(struct Interface *i2c, struct Timer *timer)
{
  const struct AmplifierConfig ampConfig = {
      .bus = i2c,
      .timer = timer,
      .address = 0x70,
      .rate = 0
  };

  return init(Amplifier, &ampConfig);
}
/*----------------------------------------------------------------------------*/
struct Entity *boardMakeCodec(struct Interface *i2c, struct Timer *timer)
{
  const struct TLV320AIC3xConfig codecConfig = {
      .bus = i2c,
      .timer = timer,
      .address = 0x18,
      .rate = 0,
      .prescaler = 0,
      .reset = BOARD_CODEC_RESET_PIN
  };

  return init(TLV320AIC3x, &codecConfig);
}
/*----------------------------------------------------------------------------*/
struct Timer *boardMakeCodecTimer(void)
{
  static const struct GpTimerConfig codecTimerConfig = {
      .frequency = 1000000,
      .priority = PRI_TIMER_I2C,
      .channel = 2
  };

  return init(GpTimer, &codecTimerConfig);
}
/*----------------------------------------------------------------------------*/
struct Timer *boardMakeLoadTimer(void)
{
  static const struct GpTimerConfig loadTimerConfig = {
      .frequency = 1000000,
      .priority = PRI_TIMER_DBG,
      .channel = 1
  };

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
  static const struct I2CConfig i2cConfig = {
      .rate = 400000, /* Initial rate */
      .scl = PIN(PORT_I2C, PIN_I2C0_SCL),
      .sda = PIN(PORT_I2C, PIN_I2C0_SDA),
      .priority = PRI_I2C,
      .channel = 0
  };

  return init(I2C, &i2cConfig);
}
/*----------------------------------------------------------------------------*/
struct Interface *boardMakeI2S(void)
{
  static const struct I2SDmaConfig i2sConfig = {
      .size = 2,
      .rate = 44100,
      .width = I2S_WIDTH_16,
      .tx = {
          .sda = PIN(PORT_7, 2),
          .sck = PIN(PORT_4, 7),
          .ws = PIN(PORT_7, 1),
          .mclk = PIN(PORT_CLK, 2),
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

  return init(I2SDma, &i2sConfig);
}
/*----------------------------------------------------------------------------*/
struct Interface *boardMakeSDMMC(void)
{
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

  return init(Sdmmc, &sdmmcConfig);
}
/*----------------------------------------------------------------------------*/
struct Interface *boardMakeSerial(void)
{
  static const struct SerialConfig serialConfig = {
      .rxLength = 16,
      .txLength = 128,
      .rate = 115200,
      .rx = PIN(PORT_1, 14),
      .tx = PIN(PORT_5, 6),
      .priority = PRI_SERIAL,
      .channel = 1
  };

  return init(Serial, &serialConfig);
}
/*----------------------------------------------------------------------------*/
struct Watchdog *boardMakeWatchdog(void)
{
  static const struct WdtConfig wdtConfig = {
      .period = 1000
  };

  return init(Wdt, &wdtConfig);
}
/*----------------------------------------------------------------------------*/
bool boardSetupAnalogPackage(struct AnalogPackage *package)
{
  static const PinNumber adcPins[] = {
      BOARD_ADC_PIN,
      0
  };
  static const struct AdcDmaConfig adcConfig = {
      .pins = adcPins,
      .event = ADC_CTOUT_15,
      .channel = 0,
      .dma = 2
  };
  static const struct GpTimerConfig adcTimerConfig = {
      .frequency = 1000000,
      .event = GPTIMER_MATCH3,
      .channel = 3
  };

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
  static const PinNumber busPins[] = {
      BOARD_BUTTON_1_PIN,
      BOARD_BUTTON_2_PIN,
      BOARD_BUTTON_3_PIN,
      BOARD_BUTTON_4_PIN,
      0
  };
  static const struct SimpleGpioBusConfig busConfig = {
      .pins = busPins,
      .direction = PIN_INPUT,
      .pull = PIN_PULLUP
  };

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
bool boardSetupCodecPackage(struct CodecPackage *package)
{
  package->baseTimer = NULL;
  package->factory = NULL;

  package->ampTimer = NULL;
  package->amp = NULL;
  package->codecTimer = NULL;
  package->codec = NULL;

  package->i2c = boardMakeI2C();
  if (package->i2c == NULL)
    return false;

  package->factory = 0;

  package->baseTimer = boardMakeCodecTimer();
  if (package->baseTimer == NULL)
    return false;
  timerSetOverflow(package->baseTimer,
      timerGetFrequency(package->baseTimer) / 1000);

  const struct TimerFactoryConfig timerFactoryConfig = {
      .timer = package->baseTimer
  };
  package->factory = init(TimerFactory, &timerFactoryConfig);
  if (package->factory == NULL)
    return false;

  package->ampTimer = timerFactoryCreate(package->factory);
  if (package->ampTimer == NULL)
    return false;
  package->amp = boardMakeAmp(package->i2c, package->ampTimer);
  if (package->amp == NULL)
    return false;

  package->codecTimer = timerFactoryCreate(package->factory);
  if (package->codecTimer == NULL)
    return false;
  package->codec = boardMakeCodec(package->i2c, package->codecTimer);
  if (package->codec == NULL)
    return false;

  bool ready = bhInit(&package->handler, 2, WQ_LP);

  ready = ready && bhAttach(&package->handler, package->amp,
      ampSetErrorCallback, ampSetIdleCallback, ampSetUpdateCallback,
      ampUpdate);

  ready = ready && bhAttach(&package->handler, package->codec,
      codecSetErrorCallback, codecSetIdleCallback, codecSetUpdateCallback,
      codecUpdate);

  return ready;
}
/*----------------------------------------------------------------------------*/
bool boardSetupClock(void)
{
  static const struct ExternalOscConfig extOscConfig = {
      .frequency = 12000000
  };
  static const struct PllConfig sysPllConfig = {
      .divisor = 4,
      .multiplier = 17,
      .source = CLOCK_EXTERNAL
  };
  static const uint32_t SDIO_MAX_FREQUENCY = 51000000;

  const bool loaded = loadClockSettings(&sharedClockSettings);

  if (!loaded)
  {
    clockEnable(MainClock, &(struct GenericClockConfig){CLOCK_INTERNAL});

    if (clockEnable(ExternalOsc, &extOscConfig) != E_OK)
      return false;
    while (!clockReady(ExternalOsc));

    if (clockEnable(SystemPll, &sysPllConfig) != E_OK)
      return false;
    while (!clockReady(SystemPll));
  }

  /* Divider D may be used by bootloader to configure SPIM clock */

  /* SDIO */
  const uint32_t frequency = clockFrequency(SystemPll);
  const struct GenericDividerConfig divConfig = {
      .divisor = (frequency + SDIO_MAX_FREQUENCY - 1) / SDIO_MAX_FREQUENCY,
      .source = CLOCK_PLL
  };

  if (clockEnable(DividerC, &divConfig) != E_OK)
    return false;
  while (!clockReady(DividerC));

  clockEnable(SdioClock, &(struct GenericClockConfig){CLOCK_IDIVC});
  while (!clockReady(SdioClock));

  /* I2S, I2C */
  clockEnable(Apb1Clock, &(struct GenericClockConfig){CLOCK_PLL});
  while (!clockReady(Apb1Clock));

  /* ADC0 */
  clockEnable(Apb3Clock, &(struct GenericClockConfig){CLOCK_PLL});
  while (!clockReady(Apb3Clock));

  /* UART1 */
  clockEnable(Uart1Clock, &(struct GenericClockConfig){CLOCK_PLL});
  while (!clockReady(Uart1Clock));

  if (!loaded)
    clockEnable(MainClock, &(struct GenericClockConfig){CLOCK_PLL});

  return true;
}

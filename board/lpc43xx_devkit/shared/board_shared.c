/*
 * board/lpc43xx_devkit/shared/board_shared.c
 * Copyright (C) 2022 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "amplifier.h"
#include "board_shared.h"
#include <dpm/audio/tlv320aic3x.h>
#include <dpm/bus_handler.h>
#include <dpm/button_complex.h>
#include <halm/core/cortex/systick.h>
#include <halm/delay.h>
#include <halm/generic/timer_factory.h>
#include <halm/platform/lpc/adc_dma.h>
#include <halm/platform/lpc/clocking.h>
#include <halm/platform/lpc/gptimer.h>
#include <halm/platform/lpc/i2c.h>
#include <halm/platform/lpc/i2s_dma.h>
#include <halm/platform/lpc/pin_int.h>
#include <halm/platform/lpc/sdmmc.h>
#include <halm/platform/lpc/serial.h>
#include <halm/platform/lpc/serial_dma.h>
#include <halm/platform/lpc/wdt.h>
#include <string.h>
/*----------------------------------------------------------------------------*/
#define DMA_SERIAL_ENABLE

#define DMA_ADC       2
#define DMA_I2S_RX    1
#define DMA_I2S_TX    0
#define DMA_SERIAL_RX 3
#define DMA_SERIAL_TX 4
/*----------------------------------------------------------------------------*/
#define PRI_TIMER_DBG 2

#define PRI_I2C       1
#define PRI_I2S       1
#define PRI_SDMMC     1
#define PRI_SERIAL    1
#define PRI_TIMER_SYS 1
/* GPDMA 1 */

/* WQ_LP 0 */
/*----------------------------------------------------------------------------*/
[[gnu::alias("boardMakeI2C1")]] struct Interface *boardMakeI2C(void);
/*----------------------------------------------------------------------------*/
[[gnu::section(".shared")]] static struct ClockSettings sharedClockSettings;
/*----------------------------------------------------------------------------*/
struct Entity *boardMakeAmp(struct Interface *i2c, struct Timer *timer)
{
  const struct AmplifierConfig ampConfig = {
      .bus = i2c,
      .timer = timer,
      .address = AMP_ADDRESS,
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
struct Timer *boardMakeChronoTimer(void)
{
  static const struct GpTimerConfig timerConfig = {
      .frequency = 1000,
      .priority = PRI_TIMER_SYS,
      .channel = 2
  };

  return init(GpTimer, &timerConfig);
}
/*----------------------------------------------------------------------------*/
struct Timer *boardMakeLoadTimer(void)
{
  static const struct GpTimerConfig timerConfig = {
      .frequency = 1000000,
      .priority = PRI_TIMER_DBG,
      .channel = 1
  };

  return init(GpTimer, &timerConfig);
}
/*----------------------------------------------------------------------------*/
struct Interface *boardMakeI2C0(void)
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
struct Interface *boardMakeI2C1(void)
{
  static const struct I2CConfig i2cConfig = {
      .rate = 400000, /* Initial rate */
      .scl = PIN(PORT_2, 4),
      .sda = PIN(PORT_2, 3),
      .priority = PRI_I2C,
      .channel = 1
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
          .dma = DMA_I2S_TX
      },
      .rx = {
          .sda = PIN(PORT_6, 2),
          .dma = DMA_I2S_RX
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
#ifdef DMA_SERIAL_ENABLE
  static const struct SerialDmaConfig serialConfig = {
      .rxChunks = 2,
      .rxLength = 32,
      .txLength = 256,
      .rate = 115200,
      .rx = PIN(PORT_1, 14),
      .tx = PIN(PORT_5, 6),
      .channel = 1,
      .dma = {DMA_SERIAL_RX, DMA_SERIAL_TX}
  };

  return init(SerialDma, &serialConfig);
#else
  static const struct SerialConfig serialConfig = {
      .rxLength = 16,
      .txLength = 256,
      .rate = 115200,
      .rx = PIN(PORT_1, 14),
      .tx = PIN(PORT_5, 6),
      .priority = PRI_SERIAL,
      .channel = 1
  };

  return init(Serial, &serialConfig);
#endif
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
      .dma = DMA_ADC
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
bool boardSetupButtonPackage(struct ButtonPackage *package,
    struct TimerFactory *factory)
{
static const struct PinIntConfig buttonIntConfigs[] = {
      {
          .pin = BOARD_BUTTON_1_PIN,
          .event = INPUT_TOGGLE,
          .pull = PIN_PULLUP
      }, {
          .pin = BOARD_BUTTON_2_PIN,
          .event = INPUT_TOGGLE,
          .pull = PIN_PULLUP
      }, {
          .pin = BOARD_BUTTON_3_PIN,
          .event = INPUT_TOGGLE,
          .pull = PIN_PULLUP
      }, {
          .pin = BOARD_BUTTON_4_PIN,
          .event = INPUT_TOGGLE,
          .pull = PIN_PULLUP
      }
  };

  static_assert(ARRAY_SIZE(buttonIntConfigs) == ARRAY_SIZE(package->buttons),
      "Incorrect button count");
  static_assert(ARRAY_SIZE(buttonIntConfigs) == ARRAY_SIZE(package->events),
      "Incorrect event count");
  static_assert(ARRAY_SIZE(buttonIntConfigs) == ARRAY_SIZE(package->timers),
      "Incorrect timer count");

  for (size_t i = 0; i < ARRAY_SIZE(buttonIntConfigs); ++i)
  {
    package->buttons[i] = NULL;
    package->events[i] = NULL;
    package->timers[i] = NULL;
  }

  for (size_t i = 0; i < ARRAY_SIZE(buttonIntConfigs); ++i)
  {
    package->events[i] = init(PinInt, &buttonIntConfigs[i]);
    if (package->events[i] == NULL)
      return false;

    package->timers[i] = timerFactoryCreate(factory);
    if (package->timers[i] == NULL)
      return false;

    const struct ButtonComplexConfig buttonConfig = {
        .interrupt = package->events[i],
        .timer = package->timers[i],
        .pin = buttonIntConfigs[i].pin,
        .delay = 3,
        .hold = 50,
        .level = false
    };
    package->buttons[i] = init(ButtonComplex, &buttonConfig);
    if (package->buttons[i] == NULL)
      return false;
  }

  return true;
}
/*----------------------------------------------------------------------------*/
bool boardSetupChronoPackage(struct ChronoPackage *package)
{
  package->factory = NULL;
  package->guardTimer = NULL;

  package->timer = init(SysTick, &(struct SysTickConfig){PRI_TIMER_SYS});
  if (package->timer == NULL)
    return false;
  timerSetOverflow(package->timer, timerGetFrequency(package->timer) / 1000);

  const struct TimerFactoryConfig timerFactoryConfig = {
      .timer = package->timer
  };
  package->factory = init(TimerFactory, &timerFactoryConfig);
  if (package->factory == NULL)
    return false;

  package->guardTimer = timerFactoryCreate(package->factory);
  if (package->guardTimer == NULL)
    return false;

  package->mountTimer = timerFactoryCreate(package->factory);
  if (package->mountTimer == NULL)
    return false;

  return true;
}
/*----------------------------------------------------------------------------*/
bool boardSetupCodecPackage(struct CodecPackage *package,
    struct TimerFactory *factory)
{
  package->ampTimer = NULL;
  package->amp = NULL;
  package->codecTimer = NULL;
  package->codec = NULL;

  package->i2c = boardMakeI2C();
  if (package->i2c == NULL)
    return false;

  package->ampTimer = timerFactoryCreate(factory);
  if (package->ampTimer == NULL)
    return false;
  package->amp = boardMakeAmp(package->i2c, package->ampTimer);
  if (package->amp == NULL)
    return false;

  package->codecTimer = timerFactoryCreate(factory);
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
  /* Divider D may be used by the bootloader for the SPIFI module */

  static const struct ExternalOscConfig extOscConfig = {
      .frequency = 12000000
  };
  static const struct GenericDividerConfig sysDivConfig = {
      .divisor = 2,
      .source = CLOCK_PLL
  };
  static const struct PllConfig sysPllConfig = {
      .divisor = 4,
      .multiplier = 17,
      .source = CLOCK_EXTERNAL
  };

  bool clockSettingsLoaded = loadClockSettings(&sharedClockSettings);
  const bool spifiClockEnabled = clockReady(SpifiClock);

  if (clockSettingsLoaded)
  {
    /* Check clock sources */
    if (!clockReady(ExternalOsc) || !clockReady(SystemPll))
      clockSettingsLoaded = false;
  }

  if (!clockSettingsLoaded)
  {
    clockEnable(MainClock, &(struct GenericClockConfig){CLOCK_INTERNAL});

    if (spifiClockEnabled)
    {
      /* Running from NOR Flash, switch SPIFI clock to IRC without disabling */
      clockEnable(SpifiClock, &(struct GenericClockConfig){CLOCK_INTERNAL});
    }

    if (clockEnable(ExternalOsc, &extOscConfig) != E_OK)
      return false;
    while (!clockReady(ExternalOsc));

    if (clockEnable(SystemPll, &sysPllConfig) != E_OK)
      return false;
    while (!clockReady(SystemPll));
  }

  static const uint32_t sdmmcMaxFrequency = 51000000;
  static const uint32_t spifiMaxFrequency = 30000000;
  const uint32_t frequency = clockFrequency(SystemPll);

  /* SDMMC */
  const struct GenericDividerConfig sdmmcDivConfig = {
      .divisor = (frequency + sdmmcMaxFrequency - 1) / sdmmcMaxFrequency,
      .source = CLOCK_PLL
  };

  if (clockEnable(DividerC, &sdmmcDivConfig) != E_OK)
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

  if (!clockSettingsLoaded)
  {
    if (sysPllConfig.divisor == 1)
    {
      /* High frequency, make a PLL clock divided by 2 for base clock ramp up */
      clockEnable(DividerA, &sysDivConfig);
      while (!clockReady(DividerA));

      clockEnable(MainClock, &(struct GenericClockConfig){CLOCK_IDIVA});
      udelay(50);
      clockEnable(MainClock, &(struct GenericClockConfig){CLOCK_PLL});

      /* Base clock is ready, temporary clock divider is not needed anymore */
      clockDisable(DividerA);
    }
    else
    {
      /* Low CPU frequency */
      clockEnable(MainClock, &(struct GenericClockConfig){CLOCK_PLL});
    }

    /* SPIFI */
    if (spifiClockEnabled)
    {
      /* Running from NOR Flash, update SPIFI clock without disabling */
      if (frequency > spifiMaxFrequency)
      {
        const struct GenericDividerConfig spifiDivConfig = {
            .divisor = (frequency + spifiMaxFrequency - 1) / spifiMaxFrequency,
            .source = CLOCK_PLL
        };

        if (clockEnable(DividerD, &spifiDivConfig) != E_OK)
          return false;
        while (!clockReady(DividerD));

        clockEnable(SpifiClock, &(struct GenericClockConfig){CLOCK_IDIVD});
      }
      else
      {
        clockEnable(SpifiClock, &(struct GenericClockConfig){CLOCK_PLL});
      }
    }
  }

  return true;
}

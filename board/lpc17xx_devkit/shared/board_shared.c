/*
 * board/lpc17xx_devkit/shared/board_shared.c
 * Copyright (C) 2022 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "amplifier.h"
#include "board_shared.h"
#include <dpm/audio/tlv320aic3x.h>
#include <dpm/button.h>
#include <halm/core/cortex/systick.h>
#include <halm/generic/sdio_spi.h>
#include <halm/generic/timer_factory.h>
#include <halm/gpio_bus.h>
#include <halm/platform/lpc/adc_dma.h>
#include <halm/platform/lpc/clocking.h>
#include <halm/platform/lpc/gptimer.h>
#include <halm/platform/lpc/i2c.h>
#include <halm/platform/lpc/i2s_dma.h>
#include <halm/platform/lpc/serial.h>
#include <halm/platform/lpc/spi.h>
#include <halm/platform/lpc/spi_dma.h>
#include <halm/platform/lpc/wdt.h>
#include <string.h>
/*----------------------------------------------------------------------------*/
#define DMA_SPI_ENABLE

#define DMA_ADC       4
#define DMA_I2S_RX    1
#define DMA_I2S_TX    0
#define DMA_SPI_RX    2
#define DMA_SPI_TX    3
/*----------------------------------------------------------------------------*/
#define PRI_TIMER_DBG 2

#define PRI_I2C       1
#define PRI_I2S       1
#define PRI_SERIAL    1
#define PRI_SPI       1
#define PRI_TIMER_MEM 1
#define PRI_TIMER_SYS 1
/* GPDMA 1 */

/* WQ_LP 0 */
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
      .samplerate = 44100,
      .prescaler = 0,
      .reset = BOARD_CODEC_RESET_PIN,
      .type = AIC3X_TYPE_3104
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
      .channel = 3
  };

  return init(GpTimer, &timerConfig);
}
/*----------------------------------------------------------------------------*/
struct Timer *boardMakeMemoryTimer(void)
{
  static const struct GpTimerConfig timerConfig = {
      .frequency = 1000000,
      .priority = PRI_TIMER_MEM,
      .channel = 0
  };

  return init(GpTimer, &timerConfig);
}
/*----------------------------------------------------------------------------*/
struct Interface *boardMakeI2C(void)
{
  static const struct I2CConfig i2cConfig = {
      .rate = 400000, /* Initial rate */
      .scl = PIN(0, 11),
      .sda = PIN(0, 10),
      .priority = PRI_I2C,
      .channel = 2
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
          .sda = PIN(0, 9),
          .sck = PIN(0, 7),
          .ws = PIN(0, 8),
          .mclk = BOARD_CODEC_CLOCK_PIN,
          .dma = DMA_I2S_TX
      },
      .rx = {
          .sda = PIN(0, 6),
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
struct Interface *boardMakeSDIO(struct Interface *spi, struct Timer *timer)
{
  const struct SdioSpiConfig config = {
      .interface = spi,
      .timer = timer,
      .wq = WQ_LP,
      .blocks = 32,
      .cs = BOARD_SDIO_CS_PIN
  };

  return init(SdioSpi, &config);
}
/*----------------------------------------------------------------------------*/
struct Interface *boardMakeSerial(void)
{
  static const struct SerialConfig serialConfig = {
      .rxLength = 16,
      .txLength = 256,
      .rate = 115200,
      .rx = 0,
      .tx = PIN(0, 15),
      .priority = PRI_SERIAL,
      .channel = 1
  };

  return init(Serial, &serialConfig);
}
/*----------------------------------------------------------------------------*/
struct Interface *boardMakeSPI(void)
{
#ifdef DMA_SPI_ENABLE
  static const struct SpiDmaConfig spiConfig = {
      .rate = 25000000,
      .miso = PIN(0, 17),
      .mosi = PIN(0, 18),
      .sck = PIN(1, 20),
      .channel = 0,
      .mode = 3,
      .dma = {DMA_SPI_RX, DMA_SPI_TX}
  };

  return init(SpiDma, &spiConfig);
#else
  static const struct SpiConfig spiConfig = {
      .rate = 25000000,
      .miso = PIN(0, 17),
      .mosi = PIN(0, 18),
      .sck = PIN(1, 20),
      .priority = PRI_SPI,
      .channel = 0,
      .mode = 3
  };

  return init(Spi, &spiConfig);
#endif
}
/*----------------------------------------------------------------------------*/
struct Watchdog *boardMakeWatchdog(void)
{
  static const struct WdtConfig wdtConfig = {
      .period = 1000,
      .source = WDT_CLOCK_IRC
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
      .frequency = 400000,
      .event = ADC_TIMER1_MAT1,
      .channel = 0,
      .dma = DMA_ADC
  };
  static const struct GpTimerConfig adcTimerConfig = {
      .frequency = 1000000,
      .event = GPTIMER_MATCH1,
      .channel = 1
  };

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

  /* Init ADC filter */
  afInit(&package->filter);
  package->value = 0;

  return true;
}
/*----------------------------------------------------------------------------*/
bool boardSetupButtonPackage(struct ButtonPackage *package,
    struct TimerFactory *factory)
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

  package->buttons = init(SimpleGpioBus, &busConfig);
  if (package->buttons == NULL)
    return false;

  package->timer = timerFactoryCreate(factory);
  if (package->timer == NULL)
    return false;

  memset(package->debounce, 0, sizeof(package->debounce));
  return true;
}
/*----------------------------------------------------------------------------*/
bool boardSetupChronoPackage(struct ChronoPackage *package)
{
  package->timer = init(SysTick, &(struct SysTickConfig){PRI_TIMER_SYS});
  if (package->timer == NULL)
    return false;
  timerSetOverflow(package->timer, timerGetFrequency(package->timer) / 1000);

  package->factory = init(TimerFactory,
      &(struct TimerFactoryConfig){package->timer});
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
  static const struct ExternalOscConfig extOscConfig = {
      .frequency = 12000000
  };
  static const struct PllConfig sysPllConfig = {
      .source = CLOCK_EXTERNAL,
      .divisor = 3,
      .multiplier = 25
  };

  if (clockEnable(ExternalOsc, &extOscConfig) != E_OK)
    return false;
  while (!clockReady(ExternalOsc));

  if (clockEnable(SystemPll, &sysPllConfig) != E_OK)
    return false;
  while (!clockReady(SystemPll));

  clockEnable(MainClock, &(struct GenericClockConfig){CLOCK_PLL});
  return true;
}

/*
 * board/lpc17xx_devkit/shared/board_shared.c
 * Copyright (C) 2022 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "board_shared.h"
#include "tlv320aic3x.h"
#include <dpm/button.h>
#include <halm/core/cortex/systick.h>
#include <halm/generic/i2c.h>
#include <halm/generic/sdio_spi.h>
#include <halm/gpio_bus.h>
#include <halm/platform/lpc/adc_dma.h>
#include <halm/platform/lpc/clocking.h>
#include <halm/platform/lpc/gptimer.h>
#include <halm/platform/lpc/i2c.h>
#include <halm/platform/lpc/i2s_dma.h>
#include <halm/platform/lpc/rit.h>
#include <halm/platform/lpc/serial.h>
#include <halm/platform/lpc/spi.h>
#include <halm/platform/lpc/spi_dma.h>
#include <halm/platform/lpc/wdt.h>
#include <string.h>
/*----------------------------------------------------------------------------*/
#define ENABLE_SPI_DMA
/*----------------------------------------------------------------------------*/
#define PRI_TIMER_DBG 2

#define PRI_TIMER_MEM 1
#define PRI_I2C       1
#define PRI_I2S       1
#define PRI_SERIAL    1
#define PRI_SPI       1
/* GPDMA 1 */

#define PRI_TIMER_SYS 0
/* WQ_LP 0 */
/*----------------------------------------------------------------------------*/
static const PinNumber adcPinArray[] = {
    BOARD_ADC_PIN,
    0
};

static const struct AdcDmaConfig adcConfig = {
    .pins = adcPinArray,
    .frequency = 400000,
    .event = ADC_TIMER1_MAT1,
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
    .event = GPTIMER_MATCH1,
    .channel = 1
};

static const struct GpTimerConfig loadTimerConfig = {
    .frequency = 1000000,
    .priority = PRI_TIMER_DBG,
    .channel = 3
};

static const struct GpTimerConfig memoryTimerConfig = {
    .frequency = 1000000,
    .priority = PRI_TIMER_MEM,
    .channel = 0
};

static const struct I2CConfig i2cConfig = {
    .rate = 400000, /* Initial rate */
    .scl = PIN(0, 11),
    .sda = PIN(0, 10),
    .priority = PRI_I2C,
    .channel = 2
};

static const struct I2SDmaConfig i2sConfig = {
    .size = 2,
    .rate = 44100,
    .width = I2S_WIDTH_16,
    .tx = {
        .sda = PIN(0, 9),
        .sck = PIN(0, 7),
        .ws = PIN(0, 8),
        .dma = 0
    },
    .rx = {
        .sda = PIN(0, 6),
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
    .rx = PIN(0, 16),
    .tx = PIN(0, 15),
    .priority = PRI_SERIAL,
    .channel = 1
};

#ifdef ENABLE_SPI_DMA
static const struct SpiDmaConfig spiConfig = {
    .rate = 25000000,
    .miso = PIN(0, 17),
    .mosi = PIN(0, 18),
    .sck = PIN(1, 20),
    .channel = 0,
    .mode = 3,
    .dma = {2, 3}
};
#else
static const struct SpiConfig spiConfig[] = {
    .rate = 25000000,
    .miso = PIN(0, 17),
    .mosi = PIN(0, 18),
    .sck = PIN(1, 20),
    .priority = PRI_SPI,
    .channel = 0,
    .mode = 3
};
#endif

static const struct WdtConfig wdtConfig = {
    .period = 2000,
    .source = WDT_CLOCK_PCLK
};
/*----------------------------------------------------------------------------*/
static const struct ExternalOscConfig extOscConfig = {
    .frequency = 12000000
};

static const struct PllConfig sysPllConfig = {
    .source = CLOCK_EXTERNAL,
    .divisor = 3,
    .multiplier = 25
};

static const struct GenericClockConfig mainClockConfig = {
    .source = CLOCK_PLL
};
/*----------------------------------------------------------------------------*/
struct Entity *boardMakeCodec(struct Interface *i2c)
{
  const struct TLV320AIC3xConfig codecConfig = {
      .interface = i2c,
      .address = 0,
      .rate = 0,
      .reset = BOARD_CODEC_RESET_PIN
  };
  struct TLV320AIC3x * const codec = init(TLV320AIC3x, &codecConfig);

  if (codec != NULL)
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
struct Timer *boardMakeMemoryTimer(void)
{
  return init(GpTimer, &memoryTimerConfig);
}
/*----------------------------------------------------------------------------*/
struct Timer *boardMakeMountTimer(void)
{
  return init(Rit, &(struct RitConfig){PRI_TIMER_SYS});
}
/*----------------------------------------------------------------------------*/
struct Interface *boardMakeI2C(void)
{
  struct Interface * const i2c = init(I2C, &i2cConfig);

  if (i2c != NULL)
    ifSetParam(i2c, IF_I2C_BUS_RECOVERY, NULL);

  return i2c;
}
/*----------------------------------------------------------------------------*/
struct Interface *boardMakeI2S(void)
{
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
  return init(Serial, &serialConfig);
}
/*----------------------------------------------------------------------------*/
struct Interface *boardMakeSPI(void)
{
#ifdef ENABLE_SPI_DMA
  return init(SpiDma, &spiConfig);
#else
  return init(Spi, &spiConfig);
#endif
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

  /* Init ADC filter */
  afInit(&package->filter);
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
  if (clockEnable(ExternalOsc, &extOscConfig) != E_OK)
    return false;
  while (!clockReady(ExternalOsc));

  if (clockEnable(SystemPll, &sysPllConfig) != E_OK)
    return false;
  while (!clockReady(SystemPll));

  if (clockEnable(MainClock, &mainClockConfig) != E_OK)
    return false;

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

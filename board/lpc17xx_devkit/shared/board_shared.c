/*
 * board/v1/shared/board_shared.c
 * Copyright (C) 2022 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "board_shared.h"
#include "tlv320aic3x.h"
#include <dpm/button.h>
#include <halm/core/cortex/systick.h>
#include <halm/generic/sdio_spi.h>
#include <halm/generic/software_timer.h>
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
#include <halm/platform/lpc/usb_device.h>
#include <string.h>
/*----------------------------------------------------------------------------*/
#define ENABLE_SPI_DMA
/*----------------------------------------------------------------------------*/
static const PinNumber adcPinArray[] = {
    PIN(0, 3),
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
    .priority = 1,
    .channel = 3
};

static const struct GpTimerConfig memoryTimerConfig = {
    .frequency = 1000000,
    .priority = 0,
    .channel = 0
};

static const struct I2CConfig i2cConfig = {
    .rate = 400000, /* Initial rate */
    .scl = PIN(0, 11),
    .sda = PIN(0, 10),
    .priority = 0,
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
        .mclk = PIN(4, 29),
        .dma = 0
    },
    .rx = {
        .sda = PIN(0, 6),
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
    .rx = PIN(0, 16),
    .tx = PIN(0, 15),
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
    .priority = 0,
    .channel = 0,
    .mode = 3
};
#endif

static const struct UsbDeviceConfig usbConfig = {
    .dm = PIN(0, 30),
    .dp = PIN(0, 29),
    .connect = PIN(2, 9),
    .vbus = PIN(1, 30),
    .vid = 0x15A2,
    .pid = 0x0044,
    .priority = 0,
    .channel = 0
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

static const struct PllConfig usbPllConfig = {
    .source = CLOCK_EXTERNAL,
    .divisor = 4,
    .multiplier = 16
};

static const struct GenericClockConfig mainClockConfig = {
    .source = CLOCK_PLL
};

static const struct GenericClockConfig usbClockConfig = {
    .source = CLOCK_USB_PLL
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
struct Timer *boardMakeMemoryTimer(void)
{
  return init(GpTimer, &memoryTimerConfig);
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
struct Interface *boardMakeSDIO(struct Interface *spi, struct Timer *timer)
{
  const struct SdioSpiConfig config = {
      .interface = spi,
      .timer = timer,
      .wq = 0,
      .blocks = 0,
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
struct Interface *boardMakeUSB(void)
{
  return init(UsbDevice, &usbConfig);
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

  /* Init ADC filter */
  afInit(&package->filter);
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
  if (clockEnable(ExternalOsc, &extOscConfig) != E_OK)
    return false;
  while (!clockReady(ExternalOsc));

  if (clockEnable(SystemPll, &sysPllConfig) != E_OK)
    return false;
  while (!clockReady(SystemPll));

  if (clockEnable(UsbPll, &usbPllConfig) != E_OK)
    return false;
  while (!clockReady(UsbPll));

  if (clockEnable(MainClock, &mainClockConfig) != E_OK)
    return false;

  if (clockEnable(UsbClock, &usbClockConfig) != E_OK)
    return false;
  while (!clockReady(UsbClock));

  return true;
}
/*----------------------------------------------------------------------------*/
// bool boardSetupTimerPackage(struct TimerPackage *package)
// {
//   /*
//    * Core frequency and SysTick resolution must be kept in mind
//    * when configuring timer overflow.
//    */
//   package->base = init(SysTickTimer, 0);
//   if (!package->base)
//     return false;
//   timerSetOverflow(package->base, timerGetFrequency(package->base) / 100);

//   const struct SoftwareTimerFactoryConfig config = {
//       .timer = package->base
//   };
//   package->factory = init(SoftwareTimerFactory, &config);
//   if (!package->factory)
//     return false;

//   return true;
// }
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

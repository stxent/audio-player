/*
 * board/v1/tests/terminal/tlv320aic3x.c
 * Copyright (C) 2022 xent
 * Project is distributed under the terms of the MIT License
 */

#include "tlv320aic3x.h"
#include "tlv320aic3x_defs.h"
#include <halm/delay.h>
#include <halm/generic/i2c.h>
#include <assert.h>
#include <string.h>
/*----------------------------------------------------------------------------*/
#define CODEC_ADDRESS 0x18
#define CODEC_RATE    400000
/*----------------------------------------------------------------------------*/
// static void codecReadBuffer(struct TLV320AIC3x *, uint16_t, void *, size_t);
// static uint8_t codecReadReg(struct TLV320AIC3x *, uint16_t);
// static uint16_t codecReadReg16(struct TLV320AIC3x *, uint16_t);
static void codecWriteBuffer(struct TLV320AIC3x *, uint16_t, const void *,
    size_t);
static void codecWriteReg(struct TLV320AIC3x *, uint16_t, uint8_t);
static void codecWriteReg16(struct TLV320AIC3x *, uint16_t, uint16_t);
static void initialConfig(struct TLV320AIC3x *);
static void setupInterface(struct TLV320AIC3x *, bool);
/*----------------------------------------------------------------------------*/
static enum Result codecInit(void *, const void *);
static void codecDeinit(void *);
/*----------------------------------------------------------------------------*/
const struct EntityClass * const TLV320AIC3x =
    &(const struct EntityClass){
    .size = sizeof(struct TLV320AIC3x),
    .init = codecInit,
    .deinit = codecDeinit
};
/*----------------------------------------------------------------------------*/
// static void codecReadBuffer(struct TLV320AIC3x *codec, uint16_t address,
//     void *buffer, size_t length)
// {
//   const uint8_t pageBuffer[2] = {0x00, (address & 0x100) ? 1 : 0};
//   size_t count;
//   bool result = false;
//
//   setupInterface(codec, false);
//   count = ifWrite(codec->interface, &pageBuffer, sizeof(pageBuffer));
//
//   if (count == sizeof(pageBuffer))
//   {
//     const uint8_t addressBuffer = (uint8_t)address;
//
//     setupInterface(codec, true);
//     count = ifWrite(codec->interface, &addressBuffer, sizeof(addressBuffer));
//
//     if (count == sizeof(addressBuffer))
//     {
//       count = ifRead(codec->interface, buffer, length);
//       result = (count == length + 1);
//     }
//   }
// }
// /*----------------------------------------------------------------------------*/
// static uint8_t codecReadReg(struct TLV320AIC3x *codec, uint16_t address)
// {
//   uint8_t buffer = 0;
//
//   codecReadBuffer(codec, address, &buffer, sizeof(buffer));
//   return buffer;
// }
// /*----------------------------------------------------------------------------*/
// static uint16_t codecReadReg16(struct TLV320AIC3x *codec, uint16_t address)
// {
//   uint16_t buffer = 0;
//
//   codecReadBuffer(codec, address, &buffer, sizeof(buffer));
//   return buffer;
// }
/*----------------------------------------------------------------------------*/
static void codecWriteBuffer(struct TLV320AIC3x *codec, uint16_t address,
    const void *buffer, size_t length)
{
  const uint8_t pageBuffer[2] = {0x00, (address & 0x100) ? 1 : 0};
  size_t count;

  setupInterface(codec, false);
  count = ifWrite(codec->interface, &pageBuffer, sizeof(pageBuffer));

  if (count == sizeof(pageBuffer))
  {
    uint8_t dataBuffer[2];

    dataBuffer[0] = (uint8_t)address;
    memcpy(dataBuffer + 1, buffer, length);

    setupInterface(codec, false);
    count = ifWrite(codec->interface, &dataBuffer, length + 1);
  }
}
/*----------------------------------------------------------------------------*/
static void codecWriteReg(struct TLV320AIC3x *codec, uint16_t address,
    uint8_t value)
{
  codecWriteBuffer(codec, address, &value, sizeof(value));
}
/*----------------------------------------------------------------------------*/
static void codecWriteReg16(struct TLV320AIC3x *codec, uint16_t address,
    uint16_t value)
{
  codecWriteBuffer(codec, address, &value, sizeof(value));
}
/*----------------------------------------------------------------------------*/
static void initialConfig(struct TLV320AIC3x *codec)
{
  /* Use PLL for 44100, P 1, R 1, J 7, D 5264 */
  codecWriteReg(codec, AIC3X_PLL_PROGB_REG, 0x1C);
  codecWriteReg16(codec, AIC3X_PLL_PROGC_REG, 0x5090);
  codecWriteReg(codec, AIC3X_OVRF_STATUS_AND_PLLR_REG, 0x01);
  codecWriteReg(codec, AIC3X_PLL_PROGA_REG, 0x91);

  /* DOUT in high-impedance on inactive bit clocks */
  codecWriteReg(codec, AIC3X_ASD_INTF_CTRLA, 0x20);

  /* 44100 Hz, left channel to left DAC, right channel to right DAC */
  codecWriteReg(codec, AIC3X_CODEC_DATAPATH_REG, 0x8A);

  /* I2S mode, 16-bit sample size */
  codecWriteReg(codec, AIC3X_ASD_INTF_CTRLB, 0x00);
}
/*----------------------------------------------------------------------------*/
static void setupInterface(struct TLV320AIC3x *codec, bool read)
{
  ifSetParam(codec->interface, IF_RATE, &codec->rate);
  ifSetParam(codec->interface, IF_ADDRESS, &codec->address);

  if (read)
    ifSetParam(codec->interface, IF_I2C_REPEATED_START, 0);
}
/*----------------------------------------------------------------------------*/
static enum Result codecInit(void *object, const void *arguments)
{
  const struct TLV320AIC3xConfig * const config = arguments;
  assert(config);

  struct TLV320AIC3x * const codec = object;

  // codec->reset = pinInit(config->reset);
  // assert(pinValid(codec->reset));
  // pinOutput(codec->reset, true);

  codec->interface = config->interface;
  codec->address = config->address ? config->address : CODEC_ADDRESS;
  codec->rate = config->rate ? config->rate : CODEC_RATE;

  // pinReset(codec->reset);
  // mdelay(10);
  // pinSet(codec->reset);

  initialConfig(codec);

  return E_OK;
}
/*----------------------------------------------------------------------------*/
static void codecDeinit(void *object)
{
  struct TLV320AIC3x * const codec = object;
  // pinOutput(codec->reset, false);
}
/*----------------------------------------------------------------------------*/
void aic3xDisablePath(struct TLV320AIC3x *codec, enum AIC3xPath path)
{
  (void)codec; // TODO
  (void)path; // TODO
}
/*----------------------------------------------------------------------------*/
void aic3xEnablePath(struct TLV320AIC3x *codec, enum AIC3xPath path)
{
  if (path == AIC3X_HP_LINE_OUT)
  {
    /* Output common-mode voltage = 1.5 V */
    codecWriteReg(codec, HPOUT_SC, 0x40);

    /* Short-circuit protection */
    codecWriteReg(codec, HPRCOM_CFG, 0x04);

    /* Power-up DAC_L, HPLCOM differential to HPLOUT */
    codecWriteReg(codec, HPLCOM_CFG, 0x80);
    /* Power-up both DAC, HPLCOM differential to HPLOUT */
    // codecWriteReg(codec, HPLCOM_CFG, 0xC0);

    /* Both DAC to high-power output drivers */
    // codecWriteReg(codec, DAC_LINE_MUX, 0xA0);
    /* DAC_L to high-power output driver */
    codecWriteReg(codec, DAC_LINE_MUX, 0x80);

    /* Unmute DAC_L */
    codecWriteReg(codec, LDAC_VOL, 0);
    /* Unmute DAC_R */
    // codecWriteReg(codec, RDAC_VOL, 0);

    /* HPLOUT unmute */
    codecWriteReg(codec, HPLOUT_CTRL, 0x09);
  }
  else if (path == AIC3X_LINE_OUT)
  {
    /* Power-up both DAC, HPLCOM differential to HPLOUT */
    codecWriteReg(codec, HPLCOM_CFG, 0xC0);
    /* Unmute DAC_L */
    codecWriteReg(codec, LDAC_VOL, 0);
    /* Unmute DAC_R */
    codecWriteReg(codec, RDAC_VOL, 0);
    /* Left DAC to DAC_L1, Right DAC to DAC_R1 */
    codecWriteReg(codec, DAC_LINE_MUX, 0x00);
    /* DAC_L1 to LEFT_LOP/M, -50 dB */
    codecWriteReg(codec, DACL1_2_LLOPM_VOL, 0xE4);
    /* DAC_R1 to RIGHT_LOP/M, -50 dB */
    codecWriteReg(codec, DACR1_2_RLOPM_VOL, 0xE4);
    /* LEFT_LOP/M unmute */
    codecWriteReg(codec, LLOPM_CTRL, 0x09);
    /* RIGHT_LOP/M unmute */
    codecWriteReg(codec, RLOPM_CTRL, 0x09);
  }
  else if (path == AIC3X_MIC_IN)
  {

  }
  else if (path == AIC3X_LINE_IN)
  {

  }
}
/*----------------------------------------------------------------------------*/
int aic3xGetGain(struct TLV320AIC3x *codec, enum AIC3xPath path)
{
  (void)codec; // TODO
  (void)path; // TODO

  return 0;
}
/*----------------------------------------------------------------------------*/
void aic3xSetGain(struct TLV320AIC3x *codec, enum AIC3xPath path, int gain)
{
  if (path == AIC3X_HP_LINE_OUT)
  {
    codecWriteReg(codec, LDAC_VOL, gain * 2);
    codecWriteReg(codec, HPLOUT_CTRL, 0x09);
  }
  else if (path == AIC3X_LINE_OUT)
  {
    codecWriteReg(codec, DACL1_2_LLOPM_VOL, 0x80 + gain * 2);
    codecWriteReg(codec, DACR1_2_RLOPM_VOL, 0x80 + gain * 2);
  }
}
/*----------------------------------------------------------------------------*/
void aic3xSetRate(struct TLV320AIC3x *codec, unsigned int rate)
{
  (void)codec; // TODO
  (void)rate; // TODO
}

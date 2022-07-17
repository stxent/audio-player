/*
 * drivers/audio/tlv320aic3x.h
 * Copyright (C) 2022 xent
 * Project is distributed under the terms of the MIT License
 */

#ifndef DPM_DRIVERS_AUDIO_TLV320AIC3X_H_
#define DPM_DRIVERS_AUDIO_TLV320AIC3X_H_
/*----------------------------------------------------------------------------*/
#include <halm/pin.h>
#include <xcore/entity.h>
/*----------------------------------------------------------------------------*/
extern const struct EntityClass * const TLV320AIC3x;

enum AIC3xPath
{
  AIC3X_HP_LINE_OUT,
  AIC3X_LINE_OUT,
  AIC3X_MIC_IN,
  AIC3X_LINE_IN
};

struct TLV320AIC3xConfig
{
  /** Mandatory: management interface. */
  struct Interface *interface;
  /** Optional: codec address. */
  uint32_t address;
  /** Optional: codec management interface rate. */
  uint32_t rate;
  /** Mandatory: codec power enable pin. */
  PinNumber reset;
};

struct TLV320AIC3x
{
  struct Entity base;

  struct Interface *interface;
  struct Pin reset;
  uint32_t address;
  uint32_t rate;
};
/*----------------------------------------------------------------------------*/
BEGIN_DECLS

void aic3xDisablePath(struct TLV320AIC3x *, enum AIC3xPath);
void aic3xEnablePath(struct TLV320AIC3x *, enum AIC3xPath);
int aic3xGetGain(struct TLV320AIC3x *, enum AIC3xPath);
void aic3xSetGain(struct TLV320AIC3x *, enum AIC3xPath, int);
void aic3xSetRate(struct TLV320AIC3x *, unsigned int);

END_DECLS
/*----------------------------------------------------------------------------*/
#endif /* DPM_DRIVERS_AUDIO_TLV320AIC3X_H_ */

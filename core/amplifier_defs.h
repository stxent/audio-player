/*
 * core/amplifier_defs.h
 * Copyright (C) 2023 xent
 * Project is distributed under the terms of the MIT License
 */

#ifndef CORE_AMPLIFIER_DEFS_H_
#define CORE_AMPLIFIER_DEFS_H_
/*----------------------------------------------------------------------------*/
#include <xcore/bits.h>
/*----------------------------------------------------------------------------*/
#define AMP_ADDRESS 0x70

enum
{
  AMP_REG_RESET   = 0x00,
  AMP_REG_SYS     = 0x01,
  AMP_REG_CTL     = 0x02,
  AMP_REG_STATUS  = 0x03,
  AMP_REG_LED     = 0x04,
  AMP_REG_SW      = 0x05,
  AMP_REG_PATH    = 0x06,
  AMP_REG_MIC     = 0x07,
  AMP_REG_SPK     = 0x08
};
/*------------------Reset control register------------------------------------*/
#define AMP_RESET_RESET               BIT(0)
/*------------------System control register-----------------------------------*/
#define AMP_SYS_EXT_CLOCK             BIT(0)
#define AMP_SYS_SUSPEND               BIT(1)
#define AMP_SYS_SUSPEND_AUTO          BIT(2)
#define AMP_SYS_SAVE_CONFIG           BIT(7)
#define AMP_SYS_MASK                  (BIT_FIELD(MASK(3), 0) | BIT(7))
/*------------------Amplifier control register--------------------------------*/
#define AMP_CTL_POWER                 BIT(0)
#define AMP_CTL_GAIN0                 BIT(1)
#define AMP_CTL_GAIN1                 BIT(2)
#define AMP_CTL_MASK                  MASK(3)
/*------------------Status register-------------------------------------------*/
#define AMP_STATUS_POWER_READY        BIT(0)
/*------------------Path control register-------------------------------------*/
#define AMP_PATH_OUTPUT(value)        BIT_FIELD((value), 0)
#define AMP_PATH_OUTPUT_MASK          BIT_FIELD(MASK(2), 0)
#define AMP_PATH_OUTPUT_VALUE(reg) \
    FIELD_VALUE((reg), AMP_PATH_OUTPUT_MASK, 0)

#define AMP_PATH_INPUT(value)         BIT_FIELD((value), 2)
#define AMP_PATH_INPUT_MASK           BIT_FIELD(MASK(2), 2)
#define AMP_PATH_INPUT_VALUE(reg) \
    FIELD_VALUE((reg), AMP_PATH_INPUT_MASK, 2)

#define AMP_PATH_MASK                 MASK(4)
/*----------------------------------------------------------------------------*/
#endif /* CORE_AMPLIFIER_DEFS_H_ */

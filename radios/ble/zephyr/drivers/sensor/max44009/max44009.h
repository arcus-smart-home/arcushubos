/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _SENSOR_MAX44009
#define _SENSOR_MAX44009

#include <misc/util.h>

#define SYS_LOG_DOMAIN "MAX44009"
#define SYS_LOG_LEVEL CONFIG_SYS_LOG_SENSOR_LEVEL
#include <logging/sys_log.h>

#define MAX44009_I2C_ADDRESS	CONFIG_MAX44009_I2C_ADDR

#define MAX44009_SAMPLING_CONTROL_BIT	BIT(7)
#define MAX44009_CONTINUOUS_SAMPLING	BIT(7)
#define MAX44009_SAMPLE_EXPONENT_SHIFT	12
#define MAX44009_MANTISSA_HIGH_NIBBLE_MASK	0xf00
#define MAX44009_MANTISSA_LOW_NIBBLE_MASK	0xf

#define MAX44009_REG_CONFIG		0x02
#define MAX44009_REG_LUX_HIGH_BYTE	0x03
#define MAX44009_REG_LUX_LOW_BYTE	0x04

struct max44009_data {
	struct device *i2c;
	u16_t sample;
};

#endif /* _SENSOR_MAX44009_ */

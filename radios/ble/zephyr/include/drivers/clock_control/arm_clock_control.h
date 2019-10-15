/*
 * Copyright (c) 2016 Linaro Limited.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _ARM_CLOCK_CONTROL_H_
#define _ARM_CLOCK_CONTROL_H_

#include <clock_control.h>

/**
 * @file
 *
 * @brief Clock subsystem IDs for ARM family SoCs
 */

/* CMSDK BUS Mapping */
enum arm_bus_type_t {
	CMSDK_AHB = 0,
	CMSDK_APB,
};

/* CPU States */
enum arm_soc_state_t {
	SOC_ACTIVE = 0,
	SOC_SLEEP,
	SOC_DEEPSLEEP,
};

struct arm_clock_control_t {
	/* ARM family SoCs supported Bus types */
	enum arm_bus_type_t bus;
	/* Clock can be configured for 3 states: Active, Sleep, Deep Sleep */
	enum arm_soc_state_t state;
	/* Identifies the device on the bus */
	u32_t device;
};

#endif /* _ARM_CLOCK_CONTROL_H_ */

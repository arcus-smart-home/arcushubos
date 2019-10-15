/*
 * Copyright (c) 2016 Nordic Semiconductor ASA
 * Copyright (c) 2016 Linaro Limited
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief System/hardware module for Nordic Semiconductor nRF51 family processor
 *
 * This module provides routines to initialize and support board-level hardware
 * for the Nordic Semiconductor nRF51 family processor.
 */

#include <kernel.h>
#include <device.h>
#include <init.h>
#include <soc.h>

#ifdef CONFIG_RUNTIME_NMI
extern void _NmiInit(void);
#define NMI_INIT() _NmiInit()
#else
#define NMI_INIT()
#endif

#include "nrf.h"

#define __SYSTEM_CLOCK (16000000UL)

static bool ftpan_26(void);
static bool ftpan_59(void);

uint32_t SystemCoreClock __used = __SYSTEM_CLOCK;

static int nordicsemi_nrf51_init(struct device *arg)
{
	u32_t key;

	ARG_UNUSED(arg);

	/* Note:
	 * Magic numbers below are obtained by reading the registers
	 * when the SoC was running the SAM-BA bootloader
	 * (with reserved bits set to 0).
	 */

	key = irq_lock();

	/* Prepare the peripherals for use as indicated by the PAN 26 "System:
	 * Manual setup is required to enable the use of peripherals" found at
	 * Product Anomaly document for your device found at
	 * https://www.nordicsemi.com/. The side effect of executing these
	 * instructions in the devices that do not need it is that the new
	 * peripherals in the second generation devices (LPCOMP for example)
	 * will not be available.
	 */
	if (ftpan_26()) {
		*(volatile u32_t *)0x40000504 = 0xC007FFDF;
		*(volatile u32_t *)0x40006C18 = 0x00008000;
	}

	/* Disable PROTENSET registers under debug, as indicated by PAN 59
	 * "MPU: Reset value of DISABLEINDEBUG register is incorrect" found
	 * at Product Anomaly document for your device found at
	 * https://www.nordicsemi.com/.
	 */
	if (ftpan_59()) {
		NRF_MPU->DISABLEINDEBUG =
			MPU_DISABLEINDEBUG_DISABLEINDEBUG_Disabled <<
			MPU_DISABLEINDEBUG_DISABLEINDEBUG_Pos;
	}

	/* Install default handler that simply resets the CPU
	 * if configured in the kernel, NOP otherwise
	 */
	NMI_INIT();

	irq_unlock(key);

	return 0;
}

static bool ftpan_26(void)
{
	if ((((*(u32_t *)0xF0000FE0) & 0x000000FF) == 0x1) &&
	    (((*(u32_t *)0xF0000FE4) & 0x0000000F) == 0x0)) {
		if ((((*(u32_t *)0xF0000FE8) & 0x000000F0) == 0x00) &&
		    (((*(u32_t *)0xF0000FEC) & 0x000000F0) == 0x0)) {
			return true;
		}
		if ((((*(u32_t *)0xF0000FE8) & 0x000000F0) == 0x10) &&
		    (((*(u32_t *)0xF0000FEC) & 0x000000F0) == 0x0)) {
			return true;
		}
		if ((((*(u32_t *)0xF0000FE8) & 0x000000F0) == 0x30) &&
		    (((*(u32_t *)0xF0000FEC) & 0x000000F0) == 0x0)) {
			return true;
		}
	}

	return false;
}

static bool ftpan_59(void)
{
	if ((((*(u32_t *)0xF0000FE0) & 0x000000FF) == 0x1) &&
	    (((*(u32_t *)0xF0000FE4) & 0x0000000F) == 0x0)) {
		if ((((*(u32_t *)0xF0000FE8) & 0x000000F0) == 0x40) &&
		    (((*(u32_t *)0xF0000FEC) & 0x000000F0) == 0x0)) {
			return true;
		}
	}

	return false;
}

SYS_INIT(nordicsemi_nrf51_init, PRE_KERNEL_1, 0);

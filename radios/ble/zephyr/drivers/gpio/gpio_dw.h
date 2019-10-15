/*
 * Copyright (c) 2015 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _GPIO_DW_H_
#define _GPIO_DW_H_

#include <zephyr/types.h>
#include <gpio.h>
#include "gpio_dw_registers.h"

#ifdef CONFIG_PCI
#include <pci/pci.h>
#include <pci/pci_mgr.h>
#endif /* CONFIG_PCI */

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*gpio_config_irq_t)(struct device *port);

struct gpio_dw_config {
	u32_t bits;
	u32_t irq_num; /* set to 0 if GPIO port cannot interrupt */
	gpio_config_irq_t config_func;

#ifdef CONFIG_GPIO_DW_SHARED_IRQ
	char *shared_irq_dev_name;
#endif /* CONFIG_GPIO_DW_SHARED_IRQ */

#ifdef CONFIG_GPIO_DW_CLOCK_GATE
	void *clock_data;
#endif
};

struct gpio_dw_runtime {
	u32_t base_addr;
#ifdef CONFIG_PCI
	struct pci_dev_info  pci_dev;
#endif /* CONFIG_PCI */

#ifdef CONFIG_GPIO_DW_CLOCK_GATE
	struct device *clock;
#endif
	sys_slist_t callbacks;
#ifdef CONFIG_DEVICE_POWER_MANAGEMENT
	u32_t device_power_state;
#endif
};

#ifdef __cplusplus
}
#endif

#endif /* _GPIO_DW_H_ */

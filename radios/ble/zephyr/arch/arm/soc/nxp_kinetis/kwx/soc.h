/*
 * Copyright (c) 2017, NXP
 * Copyright (c) 2017, Phytec Messtechnik GmbH
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _SOC__H_
#define _SOC__H_

#include <misc/util.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(CONFIG_SOC_MKW40Z4) || defined(CONFIG_SOC_MKW41Z4)

#define LPUART0_CLK_SRC kCLOCK_CoreSysClk

#endif

#if defined(CONFIG_SOC_MKW22D5) || defined(CONFIG_SOC_MKW24D5)

#define PERIPH_ADDR_BASE_WDOG 0x40052000 /* Watchdog Timer module */

#endif

#ifndef _ASMLANGUAGE

#include <fsl_common.h>
#include <device.h>
#include <misc/util.h>
#include <random/rand32.h>

#endif /* !_ASMLANGUAGE */

#ifdef __cplusplus
}
#endif

#endif /* _SOC__H_ */

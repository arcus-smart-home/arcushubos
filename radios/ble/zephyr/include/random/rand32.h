/*
 * Copyright (c) 2013-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Random number generator header file
 *
 * This header file declares prototypes for the kernel's random number generator
 * APIs.
 *
 * Typically, a platform enables the hidden CUSTOM_RANDOM_GENERATOR or
 * (for testing purposes only) enables the TEST_RANDOM_GENERATOR
 * configuration option and provide its own driver that implements
 * sys_rand32_get().
 */

#ifndef __INCrand32h
#define __INCrand32h

#include <zephyr/types.h>

#ifdef __cplusplus
extern "C" {
#endif

extern u32_t sys_rand32_get(void);

#ifdef __cplusplus
}
#endif

#endif /* __INCrand32h */

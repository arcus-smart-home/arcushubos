/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 * Copyright (c) 2018 Ioannis Glaropoulos
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define HAL_RADIO_NS2US_CEIL(ns)  ((ns + 999)/1000)
#define HAL_RADIO_NS2US_ROUND(ns) ((ns + 500)/1000)

#define EVENT_TIMER NRF_TIMER0

#if defined(CONFIG_BOARD_NRFXX_NWTSIM)
#define EVENT_TIMER_NBR 0
#endif /* CONFIG_BOARD_NRFXX_NWTSIM */

/* EVENTS_TIMER capture register used for sampling TIMER time-stamps. */
#define HAL_EVENT_TIMER_SAMPLE_CC_OFFSET 3

#if defined(CONFIG_SOC_SERIES_NRF51X)
#include "radio_nrf51.h"
#elif defined(CONFIG_SOC_NRF52832)
#include "radio_nrf52832.h"
#elif defined(CONFIG_SOC_NRF52840)
#include "radio_nrf52840.h"
#endif

#include "radio_nrf5_ppi.h"

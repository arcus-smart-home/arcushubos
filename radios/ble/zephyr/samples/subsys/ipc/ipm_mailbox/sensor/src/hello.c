/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr.h>

#include <ipm.h>
#include <ipm/ipm_quark_se.h>
#include <device.h>
#include <init.h>
#include <misc/printk.h>
#include <string.h>

QUARK_SE_IPM_DEFINE(ping_ipm, 0, QUARK_SE_IPM_INBOUND);
QUARK_SE_IPM_DEFINE(message_ipm0, 1, QUARK_SE_IPM_INBOUND);
QUARK_SE_IPM_DEFINE(message_ipm1, 2, QUARK_SE_IPM_INBOUND);
QUARK_SE_IPM_DEFINE(message_ipm2, 3, QUARK_SE_IPM_INBOUND);

/* specify delay between greetings (in ms); compute equivalent in ticks */

#define SLEEPTIME  1100

#define STACKSIZE 2000

u8_t counters[3];

void ping_ipm_callback(void *context, u32_t id, volatile void *data)
{
	printk("counters: %d %d %d\n", counters[0], counters[1], counters[2]);
}

static const char dat1[] = "abcdefghijklmno";
static const char dat2[] = "pqrstuvwxyz0123";


void message_ipm_callback(void *context, u32_t id, volatile void *data)
{
	u8_t *counter = (u8_t *)context;
	char *datac = (char *)data;
	const char *expected;

	if (*counter != id) {
		printk("expected %d got %d\n", *counter, id);
	}

	if (id & 0x1) {
		expected = dat2;
	} else {
		expected = dat1;
	}

	if (strcmp(expected, datac)) {
		printk("unexpected data payload\n");
	}
	(*counter)++;
}

void main(void)
{
	struct device *ipm;

	ipm = device_get_binding("ping_ipm");
	ipm_register_callback(ipm, ping_ipm_callback, NULL);
	ipm_set_enabled(ipm, 1);

	ipm = device_get_binding("message_ipm0");
	ipm_register_callback(ipm, message_ipm_callback, &counters[0]);
	ipm_set_enabled(ipm, 1);

	ipm = device_get_binding("message_ipm1");
	ipm_register_callback(ipm, message_ipm_callback, &counters[1]);
	ipm_set_enabled(ipm, 1);

	ipm = device_get_binding("message_ipm2");
	ipm_register_callback(ipm, message_ipm_callback, &counters[2]);
	ipm_set_enabled(ipm, 1);


	while (1) {
		/* say "hello" */
		printk("Hello from ARC!\n");

		k_sleep(SLEEPTIME);
	}
}

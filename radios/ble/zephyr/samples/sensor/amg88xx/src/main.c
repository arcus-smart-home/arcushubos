/*
 * Copyright (c) 2017 PHYTEC Messtechnik GmbH
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <device.h>
#include <sensor.h>
#include <misc/printk.h>

static struct sensor_value temp_value[64];

#ifdef CONFIG_AMG88XX_TRIGGER
K_SEM_DEFINE(sem, 0, 1);

static void trigger_handler(struct device *dev, struct sensor_trigger *trigger)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(trigger);

	k_sem_give(&sem);
}
#endif


void print_buffer(void *ptr, size_t l)
{
	struct sensor_value *tv = ptr;
	int ln = 0;

	printk("---|");
	for (int i = 0; i < 8; i++) {
		printk("  %02d  ", i);
	}
	printk("\n");

	printk("%03d|", ln);
	for (int i = 0; i < l; i++) {
		printk("%05d ", (tv[i].val1 * 100 + tv[i].val2 / 10000));
		if (!((i + 1) % 8)) {
			printk("\n");
			ln++;
			printk("%03d|", ln);
		}
	}
	printk("\n");
}

void main(void)
{
	int ret;
	struct device *dev = device_get_binding(CONFIG_AMG88XX_NAME);

	if (dev == NULL) {
		printk("Could not get AMG88XX device\n");
		return;
	}

	printk("device: %p, name: %s\n", dev, dev->config->name);

#ifdef CONFIG_AMG88XX_TRIGGER
	struct sensor_value attr = {
		.val1 = 27,
		.val2 = 0,
	};

	if (sensor_attr_set(dev, SENSOR_CHAN_AMBIENT_TEMP,
			    SENSOR_ATTR_UPPER_THRESH, &attr)) {
		printk("Could not set threshold\n");
		return;
	}

	struct sensor_trigger trig = {
		.type = SENSOR_TRIG_THRESHOLD,
		.chan = SENSOR_CHAN_AMBIENT_TEMP,
	};

	if (sensor_trigger_set(dev, &trig, trigger_handler)) {
		printk("Could not set trigger\n");
		return;
	}
#endif

	while (42) {
#ifdef CONFIG_AMG88XX_TRIGGER
		printk("Waiting for a threshold event\n");
		k_sem_take(&sem, K_FOREVER);
#endif

		ret = sensor_sample_fetch(dev);
		if (ret) {
			printk("Failed to fetch a sample, %d\n", ret);
			return;
		}

		ret = sensor_channel_get(dev, SENSOR_CHAN_AMBIENT_TEMP,
					 (struct sensor_value *)temp_value);
		if (ret) {
			printk("Failed to get sensor values, %d\n", ret);
			return;
		}

		printk("new sample:\n");
		print_buffer(temp_value, ARRAY_SIZE(temp_value));

		k_sleep(1000);
	}
}

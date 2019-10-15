/*
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 *
 * @brief Sample app to illustrate i2c master-slave communication on Intel_S1000.
 *
 * Intel_S1000 - Xtensa
 * --------------------
 *
 * The i2c_dw driver is being used.
 *
 * In this sample app, the Intel_S1000 master I2C communicates with 2 slave
 * LED I2C matrices driving them to emit blue light and red light alternately.
 * While this validates the write functionality, the read functionality is
 * verified by reading the LED0 values after each write. It would display
 * the below message repeatedly on the console every 500ms.
 *
 * "
 *     Reading LED_0 = 41
 *     Reading LED_0 = 10
 * "
 */

#include <zephyr.h>
#include <misc/printk.h>

#include <device.h>
#include <i2c.h>

#define I2C_DEV                 CONFIG_I2C_0_NAME
#define I2C_ADDR_LED_MAT0       0x65
#define I2C_ADDR_LED_MAT1       0x69
#define LED0                    0x02
#define LED1                    0x03
#define LED2                    0x04
#define LED3                    0x05
#define LED4                    0x06
#define LED5                    0x07

/* size of stack area used by each thread */
#define STACKSIZE               1024

/* scheduling priority used by each thread */
#define PRIORITY                7

/* delay between greetings (in ms) */
#define SLEEPTIME               500

extern struct k_sem thread_sem;

void test_i2c_write_led(struct device *i2c_dev, u16_t i2c_slave_led, u8_t color)
{
	int ret;
	int led_val[6];

	switch (color) {
	case 0: /* RED color LED */
		led_val[0] = 0x10;
		led_val[1] = 0x04;
		led_val[2] = 0x41;
		led_val[3] = 0x10;
		led_val[4] = 0x04;
		led_val[5] = 0x41;
		break;

	case 1: /* BLUE color LED */
		led_val[0] = 0x41;
		led_val[1] = 0x10;
		led_val[2] = 0x04;
		led_val[3] = 0x41;
		led_val[4] = 0x10;
		led_val[5] = 0x04;
		break;

	default:
		break;
	}

	ret = i2c_reg_write_byte(i2c_dev, i2c_slave_led, 0x40, 0xFF);
	ret |= i2c_reg_write_byte(i2c_dev, i2c_slave_led, LED0, led_val[0]);
	ret |= i2c_reg_write_byte(i2c_dev, i2c_slave_led, LED1, led_val[1]);
	ret |= i2c_reg_write_byte(i2c_dev, i2c_slave_led, LED2, led_val[2]);
	ret |= i2c_reg_write_byte(i2c_dev, i2c_slave_led, LED3, led_val[3]);
	ret |= i2c_reg_write_byte(i2c_dev, i2c_slave_led, LED4, led_val[4]);
	ret |= i2c_reg_write_byte(i2c_dev, i2c_slave_led, LED5, led_val[5]);
	if (ret) {
		printk("Error writing to LED!\n");
		return;
	}
}

void test_i2c_read_led(struct device *i2c_dev, u16_t i2c_slave_led)
{
	int ret;
	u8_t data = 0;

	ret = i2c_reg_read_byte(i2c_dev, i2c_slave_led, LED0, &data);
	if (ret) {
		printk("Error reading from LED! error code (%d)\n", ret);
		return;
	}
	printk("LED0 = %x\n", data);
}

/* i2c_thread is a static thread that is spawned automatically */
void i2c_thread(void *dummy1, void *dummy2, void *dummy3)
{
	struct device *i2c_dev;
	int toggle = 1;

	ARG_UNUSED(dummy1);
	ARG_UNUSED(dummy2);
	ARG_UNUSED(dummy3);

	i2c_dev = device_get_binding(I2C_DEV);
	if (!i2c_dev) {
		printk("I2C: Device driver not found.\n");
		return;
	}

	while (1) {
		/* take semaphore */
		k_sem_take(&thread_sem, K_FOREVER);

		if (toggle) {
			toggle = 0;
		} else {
			toggle = 1;
		}

		test_i2c_write_led(i2c_dev, I2C_ADDR_LED_MAT0, toggle);
		test_i2c_write_led(i2c_dev, I2C_ADDR_LED_MAT1, toggle);
		test_i2c_read_led(i2c_dev, I2C_ADDR_LED_MAT0);

		/* let other threads have a turn */
		k_sem_give(&thread_sem);

		/* wait a while */
		k_sleep(SLEEPTIME);
	}
}

K_THREAD_DEFINE(i2c_thread_id, STACKSIZE, i2c_thread, NULL, NULL, NULL,
		PRIORITY, 0, K_NO_WAIT);

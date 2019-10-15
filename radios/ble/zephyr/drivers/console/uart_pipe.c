/** @file
 * @brief Pipe UART driver
 *
 * A pipe UART driver allowing application to handle all aspects of received
 * protocol data.
 */

/*
 * Copyright (c) 2015 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <kernel.h>

#include <board.h>
#include <uart.h>

#include <console/uart_pipe.h>
#include <misc/printk.h>

static struct device *uart_pipe_dev;

static u8_t *recv_buf;
static size_t recv_buf_len;
static uart_pipe_recv_cb app_cb;
static size_t recv_off;

static void uart_pipe_rx(struct device *dev)
{
	/* As per the API, the interrupt may be an edge so keep
	 * reading from the FIFO until it's empty.
	 */
	for (;;) {
		int avail = recv_buf_len - recv_off;
		int got;

		got = uart_fifo_read(uart_pipe_dev, recv_buf + recv_off, avail);
		if (got <= 0) {
			break;
		}

		/*
		 * Call application callback with received data. Application
		 * may provide new buffer or alter data offset.
		 */
		recv_off += got;
		recv_buf = app_cb(recv_buf, &recv_off);
	}
}

static void uart_pipe_isr(struct device *dev)
{
	uart_irq_update(dev);

	if (uart_irq_is_pending(dev)) {
		if (uart_irq_rx_ready(dev)) {
			uart_pipe_rx(dev);
		}
	}
}

int uart_pipe_send(const u8_t *data, int len)
{
	while (len--)  {
		uart_poll_out(uart_pipe_dev, *data++);
	}

	return 0;
}

static void uart_pipe_setup(struct device *uart)
{
	u8_t c;

	uart_irq_rx_disable(uart);
	uart_irq_tx_disable(uart);

	/* Drain the fifo */
	while (uart_fifo_read(uart, &c, 1)) {
		continue;
	}

	uart_irq_callback_set(uart, uart_pipe_isr);

	uart_irq_rx_enable(uart);
}

void uart_pipe_register(u8_t *buf, size_t len, uart_pipe_recv_cb cb)
{
	recv_buf = buf;
	recv_buf_len = len;
	app_cb = cb;

	uart_pipe_dev = device_get_binding(CONFIG_UART_PIPE_ON_DEV_NAME);

	if (uart_pipe_dev != NULL) {
		uart_pipe_setup(uart_pipe_dev);
	}
}

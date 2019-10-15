/*
 * Copyright (c) 2017 Linaro Limited.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <uart.h>
#include <drivers/console/console.h>
#include <drivers/console/uart_console.h>

/* While app processes one input line, Zephyr will have another line
 * buffer to accumulate more console input.
 */
static struct console_input line_bufs[2];

static K_FIFO_DEFINE(free_queue);
static K_FIFO_DEFINE(used_queue);

char *console_getline(void)
{
	static struct console_input *cmd;

	/* Recycle cmd buffer returned previous time */
	if (cmd != NULL) {
		k_fifo_put(&free_queue, cmd);
	}

	cmd = k_fifo_get(&used_queue, K_FOREVER);
	return cmd->line;
}

void console_getline_init(void)
{
	int i;

	for (i = 0; i < sizeof(line_bufs) / sizeof(*line_bufs); i++) {
		k_fifo_put(&free_queue, &line_bufs[i]);
	}

	/* Zephyr UART handler takes an empty buffer from free_queue,
	 * stores UART input in it until EOL, and then puts it into
	 * used_queue.
	 */
	uart_register_input(&free_queue, &used_queue, NULL);
}

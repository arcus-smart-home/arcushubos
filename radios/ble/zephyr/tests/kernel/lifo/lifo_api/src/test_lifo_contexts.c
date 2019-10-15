/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_lifo.h"

#define STACK_SIZE (512 + CONFIG_TEST_EXTRA_STACKSIZE)
#define LIST_LEN 2
/**TESTPOINT: init via K_LIFO_DEFINE*/
K_LIFO_DEFINE(klifo);

struct k_lifo lifo;
static ldata_t data[LIST_LEN];

static K_THREAD_STACK_DEFINE(tstack, STACK_SIZE);
static struct k_thread tdata;
static struct k_sem end_sema;

static void tlifo_put(struct k_lifo *plifo)
{
	for (int i = 0; i < LIST_LEN; i++) {
		/**TESTPOINT: lifo put*/
		k_lifo_put(plifo, (void *)&data[i]);
	}
}

static void tlifo_get(struct k_lifo *plifo)
{
	void *rx_data;

	/*get lifo data*/
	for (int i = LIST_LEN-1; i >= 0; i--) {
		/**TESTPOINT: lifo get*/
		rx_data = k_lifo_get(plifo, K_FOREVER);
		zassert_equal(rx_data, (void *)&data[i], NULL);
	}
}

/*entry of contexts*/
static void tIsr_entry_put(void *p)
{
	tlifo_put((struct k_lifo *)p);
}

static void tIsr_entry_get(void *p)
{
	tlifo_get((struct k_lifo *)p);
}

static void tThread_entry(void *p1, void *p2, void *p3)
{
	tlifo_get((struct k_lifo *)p1);
	k_sem_give(&end_sema);
}

static void tlifo_thread_thread(struct k_lifo *plifo)
{
	k_sem_init(&end_sema, 0, 1);
	/**TESTPOINT: thread-thread data passing via lifo*/
	k_tid_t tid = k_thread_create(&tdata, tstack, STACK_SIZE,
		tThread_entry, plifo, NULL, NULL,
		K_PRIO_PREEMPT(0), 0, 0);
	tlifo_put(plifo);
	k_sem_take(&end_sema, K_FOREVER);
	k_thread_abort(tid);
}

static void tlifo_thread_isr(struct k_lifo *plifo)
{
	k_sem_init(&end_sema, 0, 1);
	/**TESTPOINT: thread-isr data passing via lifo*/
	irq_offload(tIsr_entry_put, plifo);
	tlifo_get(plifo);
}

static void tlifo_isr_thread(struct k_lifo *plifo)
{
	k_sem_init(&end_sema, 0, 1);
	/**TESTPOINT: isr-thread data passing via lifo*/
	tlifo_put(plifo);
	irq_offload(tIsr_entry_get, plifo);
}

/**
 * @addtogroup kernel_lifo_tests
 * @{
 */

/**
 * @see k_lifo_init(), k_lifo_put_list()
 */
void test_lifo_thread2thread(void)
{
	/**TESTPOINT: init via k_lifo_init*/
	k_lifo_init(&lifo);
	tlifo_thread_thread(&lifo);

	/**TESTPOINT: test K_LIFO_DEFINEed lifo*/
	tlifo_thread_thread(&klifo);
}

/**
 * @see k_fifo_init(), k_lifo_put_list(), #K_LIFO_DEFINE(x)
 */
void test_lifo_thread2isr(void)
{
	/**TESTPOINT: init via k_lifo_init*/
	k_lifo_init(&lifo);
	tlifo_thread_isr(&lifo);

	/**TESTPOINT: test K_LIFO_DEFINEed lifo*/
	tlifo_thread_isr(&klifo);
}

/**
 * @see k_fifo_init(), k_lifo_put_list(), #K_LIFO_DEFINE(x)
 */
void test_lifo_isr2thread(void)
{
	/**TESTPOINT: test k_lifo_init lifo*/
	k_lifo_init(&lifo);
	tlifo_isr_thread(&lifo);

	/**TESTPOINT: test K_LIFO_DEFINE lifo*/
	tlifo_isr_thread(&klifo);
}

/**
 * @}
 */

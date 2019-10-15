/*
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <ztest.h>

#define STACK_SIZE (384 + CONFIG_TEST_EXTRA_STACKSIZE)
/* nrf 51 has lower ram, so creating less number of threads */
#if defined(CONFIG_SOC_SERIES_NRF51X) || defined(CONFIG_SOC_SERIES_STM32F3X) \
    || defined(CONFIG_SOC_EMSK_EM7D)
	#define NUM_THREAD 3
#else
	#define NUM_THREAD 10
#endif
#define ITRERATION_COUNT 5
#define BASE_PRIORITY 1

static K_THREAD_STACK_ARRAY_DEFINE(tstack, NUM_THREAD, STACK_SIZE);

/* Semaphore on which Ztest thread wait*/
static K_SEM_DEFINE(sema2, 0, NUM_THREAD);

/* Semaphore on which application threads wait*/
static K_SEM_DEFINE(sema3, 0, NUM_THREAD);

static int thread_idx;
static struct k_thread t[NUM_THREAD];

/* Application thread */
static void thread_tslice(void *p1, void *p2, void *p3)
{
	/* Print New line for last thread */
	int thread_parameter = ((int)p1 == (NUM_THREAD - 1)) ? '\n' :
			       ((int)p1 + 'A');

	while (1) {
		/* Prining alphabet corresponding to thread*/
		TC_PRINT("%c", thread_parameter);
		/* Testing if threads are execueted as per priority*/
		zassert_true(((int)p1 == thread_idx), NULL);
		thread_idx = (thread_idx + 1) % (NUM_THREAD);

		/* Realease CPU and give chance to Ztest thread to run*/
		k_sem_give(&sema2);
		/*Wait for relase of semaphore from Ztest thread*/
		k_sem_take(&sema3, K_FOREVER);
	}

}

/*test cases*/

/**
 *
 * @brief Check the behavior of preemptive threads with different priorities
 *
 * @details Create multiple threads of different priorities - all are preemptive,
 * current thread is also made preemptive. Check how the threads get chance to
 * execute based on their priorities
 */
void test_priority_scheduling(void)
{
	k_tid_t tid[NUM_THREAD];
	int old_prio = k_thread_priority_get(k_current_get());
	int count = 0;

	/* update priority for current thread*/
	k_thread_priority_set(k_current_get(),
			      K_PRIO_PREEMPT(BASE_PRIORITY - 1));

	/* Create Threads with different Priority*/
	for (int i = 0; i < NUM_THREAD; i++) {
		tid[i] = k_thread_create(&t[i], tstack[i], STACK_SIZE,
					 thread_tslice, (void *)(intptr_t) i, NULL, NULL,
					 K_PRIO_PREEMPT(BASE_PRIORITY + i), 0, 0);
	}

	while (count < ITRERATION_COUNT) {

		/* Wait for each thread to complete */
		for (int i = 0; i < NUM_THREAD; i++) {
			k_sem_take(&sema2, K_FOREVER);
		}
		/* Delay to give chance to last thread to run */
		k_sleep(1);

		/* Giving Chance to other threads to run */
		for (int i = 0; i < NUM_THREAD; i++) {
			k_sem_give(&sema3);
		}
		count++;
	}


	/* test case teardown*/
	for (int i = 0; i < NUM_THREAD; i++) {
		k_thread_abort(tid[i]);
	}
	/* Set priority of Main thread to its old value */
	k_thread_priority_set(k_current_get(), old_prio);
}

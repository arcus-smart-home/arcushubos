/*
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * @file
 *
 * This module tests that XIP performs as expected. If the first
 * task is even activated that is a good indication that XIP is
 * working. However, the test does do some some testing on
 * global variables for completeness sake.
 */

#include <ztest.h>

/* This test relies on these values being one larger than the one before */
#define TEST_VAL_1  0x1
#define TEST_VAL_2  0x2
#define TEST_VAL_3  0x3
#define TEST_VAL_4  0x4

#define XIP_TEST_ARRAY_SZ 4

extern u32_t xip_array[XIP_TEST_ARRAY_SZ];

/*
 * This array is deliberately defined outside of the scope of the main test
 * module to avoid optimization issues.
 */
u32_t xip_array[XIP_TEST_ARRAY_SZ] = {
	TEST_VAL_1, TEST_VAL_2, TEST_VAL_3, TEST_VAL_4};

void test_globals(void)
{
	int  i;

	/* Array should be filled with monotomically incrementing values */
	for (i = 0; i < XIP_TEST_ARRAY_SZ; i++) {

		/**TESTPOINT: Check if the array value is correct*/
		zassert_equal(xip_array[i], (i+1), "Array value is incorrect");
	}
}

/**test case main entry*/
void test_main(void)
{
	ztest_test_suite(xip,
		ztest_unit_test(test_globals));
	ztest_run_test_suite(xip);
}

/*
 * Copyright (c) 2017 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <entropy.h>
#include <ztest.h>

/*
 * @addtogroup t_entropy_api
 * @{
 * @defgroup t_entropy_get_entropy test_entropy_get_entropy
 * @brief TestPurpose: verify Get entropy works
 * @details
 * - Test Steps
 *   -# Read random numbers from Entropy driver.
 *   -# Verify whether buffer overflow occurred or not.
 *   -# Verify whether buffer completely filled or not.
 * - Expected Results
 *   -# Random number should be generated.
 * @}
 */

#define BUFFER_LENGTH 10
#define RECHECK_RANDOM_ENTROPY 0x10

static int random_entropy(struct device *dev, char *buffer, char num)
{
	int ret, i;
	int count = 0;

	memset(buffer, num, BUFFER_LENGTH);

	/* The BUFFER_LENGTH-1 is used so the driver will not
	 * write the last byte of the buffer. If that last
	 * byte is not 0 on return it means the driver wrote
	 * outside the passed buffer, and that should never
	 * happen.
	 */
	ret = entropy_get_entropy(dev, buffer, BUFFER_LENGTH - 1);
	if (ret) {
		TC_PRINT("Error: entropy_get_entropy failed: %d\n", ret);
		return TC_FAIL;
	}
	if (buffer[BUFFER_LENGTH - 1] != num) {
		TC_PRINT("Error: entropy_get_entropy buffer overflow\n");
		return TC_FAIL;
	}

	for (i = 0; i < BUFFER_LENGTH - 1; i++) {
		TC_PRINT("  0x%02x\n", buffer[i]);
		if (buffer[i] == num) {
			count++;
		}
	}

	if (count >= 2) {
		return RECHECK_RANDOM_ENTROPY;
	} else {
		return TC_PASS;
	}
}

/*
 * Function invokes the get_entropy callback in driver
 * to get the random data and fill to passed buffer
 */
static int get_entropy(void)
{
	struct device *dev;
	u8_t buffer[BUFFER_LENGTH] = { 0 };
	int ret;

	dev = device_get_binding(CONFIG_ENTROPY_NAME);
	if (!dev) {
		TC_PRINT("error: no random device\n");
		return TC_FAIL;
	}

	TC_PRINT("random device is %p, name is %s\n",
		 dev, dev->config->name);

	ret = random_entropy(dev, buffer, 0);

	/* Check whether 20% or more of buffer still filled with default
	 * value(0), if yes then recheck again by filling nonzero value(0xa5)
	 * to buffer. Recheck random_entropy and verify whether 20% or more
	 * of buffer filled with value(0xa5) or not.
	 */
	if (ret == RECHECK_RANDOM_ENTROPY) {
		ret = random_entropy(dev, buffer, 0xa5);
		if (ret == RECHECK_RANDOM_ENTROPY) {
			return TC_FAIL;
		} else {
			return ret;
		}
	}
	return ret;
}

static void test_entropy_get_entropy(void)
{
	zassert_true(get_entropy() == TC_PASS, NULL);
}

void test_main(void)
{
	ztest_test_suite(entropy_api,
			 ztest_unit_test(test_entropy_get_entropy));
	ztest_run_test_suite(entropy_api);
}

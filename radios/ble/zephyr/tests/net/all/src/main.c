/* main.c - Application main entry point */

/* We are just testing that this program compiles ok with all possible
 * network related Kconfig options enabled.
 */

/*
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <ztest.h>

#include <net/net_if.h>
#include <net/net_pkt.h>

static struct offload_context {
	void *none;
} offload_context_data = {
	.none = NULL
};

static struct net_if_api offload_if_api = {
	.init = NULL,
	.send = NULL,
};

NET_DEVICE_OFFLOAD_INIT(net_offload, "net_offload",
			NULL, &offload_context_data, NULL,
			CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
			&offload_if_api, 0);

static void ok(void)
{
	zassert_true(true, "This test should never fail");
}

void test_main(void)
{
	ztest_test_suite(net_compile_all_test,
			 ztest_unit_test(ok)
			 );

	ztest_run_test_suite(net_compile_all_test);
}

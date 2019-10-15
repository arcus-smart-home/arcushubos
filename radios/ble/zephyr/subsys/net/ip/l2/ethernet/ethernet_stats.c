/*
 * Copyright (c) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <kernel.h>
#include <string.h>
#include <errno.h>
#include <net/net_core.h>
#include <net/ethernet.h>

#include "net_stats.h"

#if defined(CONFIG_NET_STATISTICS_USER_API)

static int eth_stats_get(u32_t mgmt_request, struct net_if *iface,
			 void *data, size_t len)
{
	const struct ethernet_api *eth;
	size_t len_chk = 0;
	void *src = NULL;

	switch (NET_MGMT_GET_COMMAND(mgmt_request)) {
	case NET_REQUEST_STATS_CMD_GET_ETHERNET:
		if (net_if_l2(iface) != &NET_L2_GET_NAME(ETHERNET)) {
			return -ENOENT;
		}

		eth = net_if_get_device(iface)->driver_api;
		len_chk = sizeof(struct net_stats_eth);
		src = eth->stats;
		break;
	}

	if (len != len_chk || !src) {
		return -EINVAL;
	}

	memcpy(data, src, len);

	return 0;
}

NET_MGMT_REGISTER_REQUEST_HANDLER(NET_REQUEST_STATS_GET_ETHERNET,
				  eth_stats_get);

#endif /* CONFIG_NET_STATISTICS_USER_API */

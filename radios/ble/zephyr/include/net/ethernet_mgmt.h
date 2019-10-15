/*
 * Copyright (c) 2018 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Ethernet Management interface public header
 */

#ifndef __ETHERNET_MGMT_H__
#define __ETHERNET_MGMT_H_

#include <net/ethernet.h>
#include <net/net_mgmt.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Ethernet library
 * @defgroup ethernet_mgmt Ethernet Library
 * @ingroup networking
 * @{
 */

#define _NET_ETHERNET_LAYER	NET_MGMT_LAYER_L2
#define _NET_ETHERNET_CODE	0x208
#define _NET_ETHERNET_BASE	(NET_MGMT_IFACE_BIT |			\
				 NET_MGMT_LAYER(_NET_ETHERNET_LAYER) |	\
				 NET_MGMT_LAYER_CODE(_NET_ETHERNET_CODE))
#define _NET_ETHERNET_EVENT	(_NET_ETHERNET_BASE | NET_MGMT_EVENT_BIT)

enum net_request_ethernet_cmd {
	NET_REQUEST_ETHERNET_CMD_SET_AUTO_NEGOTIATION = 1,
	NET_REQUEST_ETHERNET_CMD_SET_LINK,
	NET_REQUEST_ETHERNET_CMD_SET_DUPLEX,
	NET_REQUEST_ETHERNET_CMD_SET_MAC_ADDRESS,
};

#define NET_REQUEST_ETHERNET_SET_AUTO_NEGOTIATION			\
	(_NET_ETHERNET_BASE | NET_REQUEST_ETHERNET_CMD_SET_AUTO_NEGOTIATION)

NET_MGMT_DEFINE_REQUEST_HANDLER(NET_REQUEST_ETHERNET_SET_AUTO_NEGOTIATION);

#define NET_REQUEST_ETHERNET_SET_LINK					\
	(_NET_ETHERNET_BASE | NET_REQUEST_ETHERNET_CMD_SET_LINK)

NET_MGMT_DEFINE_REQUEST_HANDLER(NET_REQUEST_ETHERNET_SET_LINK);

#define NET_REQUEST_ETHERNET_SET_DUPLEX					\
	(_NET_ETHERNET_BASE | NET_REQUEST_ETHERNET_CMD_SET_DUPLEX)

NET_MGMT_DEFINE_REQUEST_HANDLER(NET_REQUEST_ETHERNET_SET_DUPLEX);

#define NET_REQUEST_ETHERNET_SET_MAC_ADDRESS				\
	(_NET_ETHERNET_BASE | NET_REQUEST_ETHERNET_CMD_SET_MAC_ADDRESS)

NET_MGMT_DEFINE_REQUEST_HANDLER(NET_REQUEST_ETHERNET_SET_MAC_ADDRESS);

struct net_eth_addr;

struct ethernet_req_params {
	union {
		bool auto_negotiation;
		bool full_duplex;

		struct {
			bool link_10bt;
			bool link_100bt;
			bool link_1000bt;
		} l;

		struct net_eth_addr mac_address;
	};
};

enum net_event_ethernet_cmd {
	NET_EVENT_ETHERNET_CMD_CARRIER_ON = 1,
	NET_EVENT_ETHERNET_CMD_CARRIER_OFF,
};

#define NET_EVENT_ETHERNET_CARRIER_ON					\
	(_NET_ETHERNET_EVENT | NET_EVENT_ETHERNET_CMD_CARRIER_ON)

#define NET_EVENT_ETHERNET_CARRIER_OFF					\
	(_NET_ETHERNET_EVENT | NET_EVENT_ETHERNET_CMD_CARRIER_OFF)

struct net_if;

void ethernet_mgmt_raise_carrier_on_event(struct net_if *iface);

void ethernet_mgmt_raise_carrier_off_event(struct net_if *iface);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* __ETHERNET_MGMT_H__ */

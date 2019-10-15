/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TCP_CONFIG_H_
#define TCP_CONFIG_H_

#ifdef CONFIG_NET_APP_SETTINGS
#ifdef CONFIG_NET_IPV6
#define LOCAL_ADDR	CONFIG_NET_APP_MY_IPV6_ADDR
#define SERVER_ADDR	CONFIG_NET_APP_PEER_IPV6_ADDR
#else
#define LOCAL_ADDR	CONFIG_NET_APP_MY_IPV4_ADDR
#define SERVER_ADDR	CONFIG_NET_APP_PEER_IPV4_ADDR
#endif
#else
#ifdef CONFIG_NET_IPV6
#define LOCAL_ADDR	"2001:db8::1"
#define SERVER_ADDR	"2001:db8::2"
#else
#define LOCAL_ADDR	"192.0.2.1"
#define SERVER_ADDR	"192.0.2.2"
#endif
#endif /* CONFIG */

#define SERVER_PORT	4433

#define MBEDTLS_NETWORK_TIMEOUT	30000
#define TCP_RX_TIMEOUT		5000
#define TCP_TX_TIMEOUT		5000
#define APP_SLEEP_MSECS		500

#endif

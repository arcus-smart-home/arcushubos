/** @file
 @brief Generic connection handling.

 This is not to be included by the application.
 */

/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __CONNECTION_H
#define __CONNECTION_H

#include <zephyr/types.h>

#include <misc/util.h>

#include <net/net_core.h>
#include <net/net_ip.h>
#include <net/net_pkt.h>

#ifdef __cplusplus
extern "C" {
#endif

struct net_conn;

struct net_conn_handle;

/**
 * @brief Function that is called by connection subsystem when UDP/TCP
 * packet is received and which matches local and remote IP address
 * and port.
 */
typedef enum net_verdict (*net_conn_cb_t)(struct net_conn *conn,
					  struct net_pkt *pkt,
					  void *user_data);

/**
 * @brief Information about a connection in the system.
 *
 * Stores the connection information.
 *
 */
struct net_conn {
	/** Remote IP address */
	struct sockaddr remote_addr;

	/** Local IP address */
	struct sockaddr local_addr;

	/** Callback to be called when matching UDP packet is received */
	net_conn_cb_t cb;

	/** Possible user to pass to the callback */
	void *user_data;

	/** Connection protocol */
	u8_t proto;

	/** Flags for the connection */
	u8_t flags;

	/** Rank of this connection. Higher rank means more specific
	 * connection.
	 * Value is constructed like this:
	 *   bit 0  local port, bit set if specific value
	 *   bit 1  remote port, bit set if specific value
	 *   bit 2  local address, bit set if unspecified address
	 *   bit 3  remote address, bit set if unspecified address
	 *   bit 4  local address, bit set if specific address
	 *   bit 5  remote address, bit set if specific address
	 */
	u8_t rank;
};

/**
 * @brief Register a callback to be called when UDP/TCP packet
 * is received corresponding to received packet.
 *
 * @param proto Protocol for the connection (UDP or TCP)
 * @param remote_addr Remote address of the connection end point.
 * @param local_addr Local address of the connection end point.
 * @param remote_port Remote port of the connection end point.
 * @param local_port Local port of the connection end point.
 * @param cb Callback to be called
 * @param user_data User data supplied by caller.
 * @param handle Connection handle that can be used when unregistering
 *
 * @return Return 0 if the registration succeed, <0 otherwise.
 */
int net_conn_register(enum net_ip_protocol proto,
		      const struct sockaddr *remote_addr,
		      const struct sockaddr *local_addr,
		      u16_t remote_port,
		      u16_t local_port,
		      net_conn_cb_t cb,
		      void *user_data,
		      struct net_conn_handle **handle);

/**
 * @brief Unregister connection handler.
 *
 * @param handle Handle from registering.
 *
 * @return Return 0 if the unregistration succeed, <0 otherwise.
 */
int net_conn_unregister(struct net_conn_handle *handle);

/**
 * @brief Change the callback and user_data for a registered connection
 * handle.
 *
 * @param handle A handle registered with net_conn_register()
 * @param cb Callback to be called
 * @param user_data User data supplied by caller.
 *
 * @return Return 0 if the the change succeed, <0 otherwise.
 */
int net_conn_change_callback(struct net_conn_handle *handle,
			     net_conn_cb_t cb, void *user_data);

/**
 * @brief Called by net_core.c when a network packet is received.
 *
 * @param pkt Network packet holding received data
 *
 * @return NET_OK if the packet was consumed, NET_DROP if
 * the packet parsing failed and the caller should handle
 * the received packet. If corresponding IP protocol support is
 * disabled, the function will always return NET_DROP.
 */
#if defined(CONFIG_NET_UDP) || defined(CONFIG_NET_TCP)
enum net_verdict net_conn_input(enum net_ip_protocol proto,
				struct net_pkt *pkt);
#else
static inline enum net_verdict net_conn_input(enum net_ip_protocol proto,
					      struct net_pkt *pkt)
{
	return NET_DROP;
}
#endif /* CONFIG_NET_UDP || CONFIG_NET_TCP */

/**
 * @typedef net_conn_foreach_cb_t
 * @brief Callback used while iterating over network connection
 * handlers.
 *
 * @param conn A valid pointer on current network connection handler.
 * @param user_data A valid pointer on some user data or NULL
 */
typedef void (*net_conn_foreach_cb_t)(struct net_conn *conn, void *user_data);

/**
 * @brief Go through all the network connection handlers and call callback
 * for each network connection handler.
 *
 * @param cb User supplied callback function to call.
 * @param user_data User specified data.
 */
void net_conn_foreach(net_conn_foreach_cb_t cb, void *user_data);

void net_conn_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __CONNECTION_H */

/*
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <net/net_core.h>
#include <net/net_context.h>
#include <net/net_pkt.h>
#include <net/net_if.h>
#include <net/udp.h>
#include <string.h>
#include <errno.h>
#include <misc/printk.h>

#include "udp_cfg.h"
#include "udp.h"

static const socklen_t addrlen = sizeof(struct sockaddr_in6);

static void set_client_address(struct sockaddr *addr, struct net_pkt *rx_pkt)
{
	struct net_udp_hdr hdr, *udp_hdr;

	udp_hdr = net_udp_get_hdr(rx_pkt, &hdr);
	if (!udp_hdr) {
		printk("Invalid UDP data\n");
		return;
	}

	net_ipaddr_copy(&net_sin6(addr)->sin6_addr, &NET_IPV6_HDR(rx_pkt)->src);
	net_sin6(addr)->sin6_family = AF_INET6;
	net_sin6(addr)->sin6_port = udp_hdr->src_port;
}

static void udp_received(struct net_context *context,
			 struct net_pkt *pkt, int status, void *user_data)
{
	struct udp_context *ctx = user_data;

	ARG_UNUSED(context);
	ARG_UNUSED(status);

	ctx->rx_pkt = pkt;
	k_sem_give(&ctx->rx_sem);
}

int udp_tx(void *context, const unsigned char *buf, size_t size)
{
	struct udp_context *ctx = context;
	struct net_context *net_ctx;
	struct net_pkt *send_pkt;

	int rc, len;

	net_ctx = ctx->net_ctx;

	send_pkt = net_pkt_get_tx(net_ctx, K_FOREVER);
	if (!send_pkt) {
		printk("cannot create pkt\n");
		return -EIO;
	}

	rc = net_pkt_append_all(send_pkt, size, (u8_t *) buf, K_FOREVER);
	if (!rc) {
		printk("cannot write buf\n");
		return -EIO;
	}

	len = net_pkt_get_len(send_pkt);

	rc = net_context_sendto(send_pkt, &net_ctx->remote,
				addrlen, NULL, K_FOREVER, NULL, NULL);

	if (rc < 0) {
		printk("Cannot send data to peer (%d)\n", rc);
		net_pkt_unref(send_pkt);
		return -EIO;
	} else {
		return len;
	}
}

int udp_rx(void *context, unsigned char *buf, size_t size)
{
	struct udp_context *ctx = context;
	struct net_context *net_ctx = ctx->net_ctx;
	struct net_pkt *rx_pkt = NULL;
	struct net_buf *rx_buf;
	u16_t read_bytes;
	u8_t *ptr;
	int pos;
	int len;
	int rc;

	k_sem_take(&ctx->rx_sem, K_FOREVER);

	read_bytes = net_pkt_appdatalen(ctx->rx_pkt);
	if (read_bytes > size) {
		return -ENOMEM;
	}

	rx_pkt = ctx->rx_pkt;

	set_client_address(&net_ctx->remote, rx_pkt);

	ptr = net_pkt_appdata(rx_pkt);
	rx_buf = rx_pkt->frags;
	len = rx_buf->len - (ptr - rx_buf->data);
	pos = 0;

	while (rx_buf) {
		memcpy(buf + pos, ptr, len);
		pos += len;

		rx_buf = rx_buf->frags;
		if (!rx_buf) {
			break;
		}

		ptr = rx_buf->data;
		len = rx_buf->len;
	}

	net_pkt_unref(ctx->rx_pkt);
	ctx->rx_pkt = NULL;

	if (read_bytes != pos) {
		return -EIO;
	}

	rc = read_bytes;
	ctx->remaining = 0;

	return rc;
}

int udp_init(struct udp_context *ctx)
{
	struct net_context *udp_ctx = { 0 };
	struct net_context *mcast_ctx = { 0 };
	struct sockaddr_in6 my_addr = { 0 };
	struct sockaddr_in6 my_mcast_addr = { 0 };
	int rc;

	k_sem_init(&ctx->rx_sem, 0, UINT_MAX);

	net_ipaddr_copy(&my_mcast_addr.sin6_addr, &mcast_addr);
	my_mcast_addr.sin6_family = AF_INET6;

	net_ipaddr_copy(&my_addr.sin6_addr, &server_addr);
	my_addr.sin6_family = AF_INET6;
	my_addr.sin6_port = htons(SERVER_PORT);

	rc = net_context_get(AF_INET6, SOCK_DGRAM, IPPROTO_UDP, &udp_ctx);
	if (rc < 0) {
		printk("Cannot get network context for IPv6 UDP (%d)", rc);
		return -EIO;
	}

	rc = net_context_bind(udp_ctx, (struct sockaddr *)&my_addr,
			      sizeof(struct sockaddr_in6));
	if (rc < 0) {
		printk("Cannot bind IPv6 UDP port %d (%d)", SERVER_PORT, rc);
		goto error;
	}

	rc = net_context_get(AF_INET6, SOCK_DGRAM, IPPROTO_UDP, &mcast_ctx);
	if (rc < 0) {
		printk("Cannot get receiving IPv6 mcast (%d)", rc);
		goto error;
	}

	rc = net_context_bind(mcast_ctx, (struct sockaddr *)&my_mcast_addr,
			      sizeof(struct sockaddr_in6));
	if (rc < 0) {
		printk("Cannot get bind IPv6 mcast (%d)", rc);
		goto error;
	}

	ctx->rx_pkt = NULL;
	ctx->remaining = 0;
	ctx->net_ctx = udp_ctx;

	rc = net_context_recv(ctx->net_ctx, udp_received, K_NO_WAIT, ctx);
	if (rc != 0) {
		return -EIO;
	}

	return 0;

error:
	net_context_put(udp_ctx);
	return -EINVAL;
}

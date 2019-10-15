/**
 * @file
 * @brief BSD Sockets compatible API definitions
 *
 * An API for applications to use BSD Sockets like API.
 */

/*
 * Copyright (c) 2017 Linaro Limited
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __NET_SOCKET_H
#define __NET_SOCKET_H

/**
 * @brief BSD Sockets compatible API
 * @defgroup bsd_sockets BSD Sockets compatible API
 * @ingroup networking
 * @{
 */

#include <sys/types.h>
#include <zephyr/types.h>
#include <net/net_ip.h>
#include <net/dns_resolve.h>

#ifdef __cplusplus
extern "C" {
#endif

struct zsock_pollfd {
	int fd;
	short events;
	short revents;
};

/* Values are compatible with Linux */
#define ZSOCK_POLLIN 1
#define ZSOCK_POLLOUT 4

#define ZSOCK_MSG_PEEK 0x02
#define ZSOCK_MSG_DONTWAIT 0x40

struct zsock_addrinfo {
	struct zsock_addrinfo *ai_next;
	int ai_flags;
	int ai_family;
	int ai_socktype;
	int ai_protocol;
	socklen_t ai_addrlen;
	struct sockaddr *ai_addr;
	char *ai_canonname;

	struct sockaddr _ai_addr;
	char _ai_canonname[DNS_MAX_NAME_SIZE + 1];
};

int zsock_socket(int family, int type, int proto);
int zsock_close(int sock);
int zsock_bind(int sock, const struct sockaddr *addr, socklen_t addrlen);
int zsock_connect(int sock, const struct sockaddr *addr, socklen_t addrlen);
int zsock_listen(int sock, int backlog);
int zsock_accept(int sock, struct sockaddr *addr, socklen_t *addrlen);
ssize_t zsock_send(int sock, const void *buf, size_t len, int flags);
ssize_t zsock_recv(int sock, void *buf, size_t max_len, int flags);
ssize_t zsock_sendto(int sock, const void *buf, size_t len, int flags,
		     const struct sockaddr *dest_addr, socklen_t addrlen);
ssize_t zsock_recvfrom(int sock, void *buf, size_t max_len, int flags,
		       struct sockaddr *src_addr, socklen_t *addrlen);
int zsock_fcntl(int sock, int cmd, int flags);
int zsock_poll(struct zsock_pollfd *fds, int nfds, int timeout);
int zsock_inet_pton(sa_family_t family, const char *src, void *dst);
int zsock_getaddrinfo(const char *host, const char *service,
		      const struct zsock_addrinfo *hints,
		      struct zsock_addrinfo **res);

#if defined(CONFIG_NET_SOCKETS_POSIX_NAMES)
static inline int socket(int family, int type, int proto)
{
	return zsock_socket(family, type, proto);
}

static inline int close(int sock)
{
	return zsock_close(sock);
}

static inline int bind(int sock, const struct sockaddr *addr, socklen_t addrlen)
{
	return zsock_bind(sock, addr, addrlen);
}

static inline int connect(int sock, const struct sockaddr *addr,
			  socklen_t addrlen)
{
	return zsock_connect(sock, addr, addrlen);
}

static inline int listen(int sock, int backlog)
{
	return zsock_listen(sock, backlog);
}

static inline int accept(int sock, struct sockaddr *addr, socklen_t *addrlen)
{
	return zsock_accept(sock, addr, addrlen);
}

static inline ssize_t send(int sock, const void *buf, size_t len, int flags)
{
	return zsock_send(sock, buf, len, flags);
}

static inline ssize_t recv(int sock, void *buf, size_t max_len, int flags)
{
	return zsock_recv(sock, buf, max_len, flags);
}

/* This conflicts with fcntl.h, so code must include fcntl.h before socket.h: */
#define fcntl zsock_fcntl

static inline ssize_t sendto(int sock, const void *buf, size_t len, int flags,
			     const struct sockaddr *dest_addr,
			     socklen_t addrlen)
{
	return zsock_sendto(sock, buf, len, flags, dest_addr, addrlen);
}

static inline ssize_t recvfrom(int sock, void *buf, size_t max_len, int flags,
			       struct sockaddr *src_addr, socklen_t *addrlen)
{
	return zsock_recvfrom(sock, buf, max_len, flags, src_addr, addrlen);
}

static inline int poll(struct zsock_pollfd *fds, int nfds, int timeout)
{
	return zsock_poll(fds, nfds, timeout);
}

#define pollfd zsock_pollfd
#define POLLIN ZSOCK_POLLIN
#define POLLOUT ZSOCK_POLLOUT

#define MSG_PEEK ZSOCK_MSG_PEEK
#define MSG_DONTWAIT ZSOCK_MSG_DONTWAIT

static inline char *inet_ntop(sa_family_t family, const void *src, char *dst,
			      size_t size)
{
	return net_addr_ntop(family, src, dst, size);
}

static inline int inet_pton(sa_family_t family, const char *src, void *dst)
{
	return zsock_inet_pton(family, src, dst);
}

static inline int getaddrinfo(const char *host, const char *service,
			      const struct zsock_addrinfo *hints,
			      struct zsock_addrinfo **res)
{
	return zsock_getaddrinfo(host, service, hints, res);
}

static inline void freeaddrinfo(struct zsock_addrinfo *ai)
{
	ARG_UNUSED(ai);
}

#define addrinfo zsock_addrinfo
#define EAI_BADFLAGS DNS_EAI_BADFLAGS
#define EAI_NONAME DNS_EAI_NONAME
#define EAI_AGAIN DNS_EAI_AGAIN
#define EAI_FAIL DNS_EAI_FAIL
#define EAI_NODATA DNS_EAI_NODATA
#endif /* defined(CONFIG_NET_SOCKETS_POSIX_NAMES) */

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* __NET_SOCKET_H */

/*-
 * Copyright (c) 2020 Hans Petter Selasky. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "hpsjam.h"

#include "peer.h"

#include <pthread.h>
#include <err.h>

static void *
hpsjam_socket_receive(void *arg)
{
	struct hpsjam_socket_address *ps = (struct hpsjam_socket_address *)arg;
	int tries = (hpsjam_num_server_peers ? 1 : 128);
	union hpsjam_frame frame;
	ssize_t ret;

	frame.clear();

	ps->setup();

	ret = ps->socket();
	assert(ret > -1);

	while (tries--) {
		ret = ps->bind();
		if (ret > -1)
			break;
		ps->incrementPort();
	}

	if (tries < 0) {
		warn("Cannot bind to IP port");
	} else while (1) {
		ret = ps->recvfrom((char *)&frame, sizeof(frame));
		if (ret >= sizeof(frame.hdr)) {
			/* zero end of frame to avoid garbage */
			memset(frame.raw + ret, 0, sizeof(frame) - ret);
			/* process frame */
			hpsjam_peer_receive(*ps, frame);
		}
	}

	ps->cleanup();

	return (NULL);
}

bool
hpsjam_socket_address :: resolve(const char *host, const char *port, struct hpsjam_socket_address &result)
{
	struct addrinfo hints;
	struct addrinfo *res;
	struct addrinfo *res0;

	/* check if not ready */
	if (fd < 0)
		return (false);

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = v4.sin_family;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = IPPROTO_UDP;

	if (getaddrinfo(host, port, &hints, &res))
		return (false);

	result = *this;

	res0 = res;

	do {
		switch (res0->ai_family) {
		case AF_INET:
			memcpy(&result.v4, res0->ai_addr, sizeof(result.v4));
			freeaddrinfo(res);
			return (true);
		case AF_INET6:
			memcpy(&result.v6, res0->ai_addr, sizeof(result.v6));
			freeaddrinfo(res);
			return (true);
		default:
			assert(0);
		}
	} while ((res0 = res0->ai_next) != NULL);

	freeaddrinfo(res);
	return (false);
}

Q_DECL_EXPORT void
hpsjam_socket_init(unsigned short port)
{
	pthread_t pt;
	int ret;

	hpsjam_v4.init(AF_INET, port);
	ret = pthread_create(&pt, NULL, &hpsjam_socket_receive, &hpsjam_v4);
	assert(ret == 0);

	hpsjam_v6.init(AF_INET6, port);
	ret = pthread_create(&pt, NULL, &hpsjam_socket_receive, &hpsjam_v6);
	assert(ret == 0);
}

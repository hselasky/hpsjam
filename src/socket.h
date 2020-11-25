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

#ifndef	_HPSJAM_SOCKET_H_
#define	_HPSJAM_SOCKET_H_

#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#ifndef _WIN32
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>
#endif

struct hpsjam_socket_address {
	union {
		struct sockaddr_in v4;
		struct sockaddr_in6 v6;
	};
	int fd;
	bool valid() const {
		return (fd > -1);
	};
	void clear() {
		fd = -1;
		if (sizeof(v6) < sizeof(v4))
			memset(&v4, 0, sizeof(v4));
		else
			memset(&v6, 0, sizeof(v6));
	};
	void init(int family, unsigned short port = 0) {
		clear();
		switch (family) {
		case AF_INET:
			v4.sin_family = family;
			v4.sin_port = htons(port);
			break;
		case AF_INET6:
			v6.sin6_family = family;
			v6.sin6_port = htons(port);
			break;
		default:
			assert(0);
		}
	};
	bool resolve(const char *, const char *, struct hpsjam_socket_address &);
	int socket(int buffer) {
		assert(fd < 0);
		switch (v4.sin_family) {
		case AF_INET:
			fd = ::socket(AF_INET, SOCK_DGRAM, 0);
			break;
		case AF_INET6:
			fd = ::socket(AF_INET6, SOCK_DGRAM, 0);
			break;
		default:
			assert(0);
		}
		if (fd > -1) {
			setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &buffer, sizeof(buffer));
			setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &buffer, sizeof(buffer));
		}
		return (fd);
	};
	int bind() const {
		switch (v4.sin_family) {
		case AF_INET:
			return (::bind(fd, (const struct sockaddr *)&v4, sizeof(v4)));
		case AF_INET6:
			return (::bind(fd, (const struct sockaddr *)&v6, sizeof(v6)));
		default:
			assert(0);
		}
	};
	ssize_t recvfrom(char *buffer, size_t bytes) {
#ifdef _WIN32
		int addr_size;
#else
		socklen_t addr_size;
#endif
		assert(valid());
		switch (v4.sin_family) {
		case AF_INET:
			addr_size = sizeof(v4);
			return (::recvfrom(fd, buffer, bytes, 0, (struct sockaddr *)&v4, &addr_size));
		case AF_INET6:
			addr_size = sizeof(v6);
			return (::recvfrom(fd, buffer, bytes, 0, (struct sockaddr *)&v6, &addr_size));
		default:
			assert(0);
		}
	};
	ssize_t sendto(const char *buffer, size_t bytes) const {
		if (!valid())
			return (-1);
		switch (v4.sin_family) {
		case AF_INET:
			return (::sendto(fd, buffer, bytes, 0, (struct sockaddr *)&v4, sizeof(v4)));
		case AF_INET6:
			return (::sendto(fd, buffer, bytes, 0, (struct sockaddr *)&v6, sizeof(v6)));
		default:
			assert(0);
		}
	};
	void close() {
		assert(valid());
#ifdef _WIN32
		closesocket(fd);
#elif defined(__APPLE__) || defined(__MACOSX)
		::close(fd);
#else
		shutdown(fd, SHUT_RDWR);
		::close(fd);
#endif
		fd = -1;
	};
	void setup() const {
#ifdef _WIN32
		WSADATA wsa;
		WSAStartup(MAKEWORD(1, 0), &wsa);
#endif
	};
	void cleanup() const {
#ifdef _WIN32
		WSACleanup();
#endif
	};
	void incrementPort() {
		switch (v4.sin_family) {
		case AF_INET:
			v4.sin_port = htons(ntohs(v4.sin_port) + 1);
			break;
		case AF_INET6:
			v6.sin6_port = htons(ntohs(v6.sin6_port) + 1);
			break;
		default:
			assert(0);
		}
	};
	int compare(const struct hpsjam_socket_address &other) const {
		if (v4.sin_family > other.v4.sin_family)
			return (1);
		else if (v4.sin_family < other.v4.sin_family)
			return (-1);
		else switch (v4.sin_family) {
		case AF_INET:
			if (v4.sin_port > other.v4.sin_port)
				return (1);
			else if (v4.sin_port < other.v4.sin_port)
				return (-1);
			else if (v4.sin_addr.s_addr > other.v4.sin_addr.s_addr)
				return (1);
			else if (v4.sin_addr.s_addr < other.v4.sin_addr.s_addr)
				return (-1);
			else
				return (0);
		case AF_INET6:
			if (v6.sin6_port > other.v6.sin6_port)
				return (1);
			else if (v6.sin6_port < other.v6.sin6_port)
				return (-1);
			else if (((const uint64_t *)&v6.sin6_addr)[0] > ((const uint64_t *)&other.v6.sin6_addr)[0])
				return (1);
			else if (((const uint64_t *)&v6.sin6_addr)[0] < ((const uint64_t *)&other.v6.sin6_addr)[0])
				return (-1);
			else if (((const uint64_t *)&v6.sin6_addr)[1] > ((const uint64_t *)&other.v6.sin6_addr)[1])
				return (1);
			else if (((const uint64_t *)&v6.sin6_addr)[1] < ((const uint64_t *)&other.v6.sin6_addr)[1])
				return (-1);
			else
				return (0);
		default:
			assert(0);
		}
	};
	bool operator >(const struct hpsjam_socket_address &other) const {
		return (compare(other) > 0);
	};
	bool operator <(const struct hpsjam_socket_address &other) const {
		return (compare(other) < 0);
	};
	bool operator >=(const struct hpsjam_socket_address &other) const {
		return (compare(other) >= 0);
	};
	bool operator <=(const struct hpsjam_socket_address &other) const {
		return (compare(other) <= 0);
	};
	bool operator == (const struct hpsjam_socket_address &other) const {
		return (compare(other) == 0);
	};
	bool operator !=(const struct hpsjam_socket_address &other) const {
		return (compare(other) != 0);
	};
};

#endif		/* _HPSJAM_SOCKET_H_ */

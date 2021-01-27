/*-
 * Copyright (c) 2020-2021 Hans Petter Selasky. All rights reserved.
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

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>
#include <poll.h>
#include <assert.h>

#ifdef __linux__
#include <linux/sockios.h>
#endif

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/uio.h>

#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include <pthread.h>

#include "hpsjam.h"
#include "httpd.h"
#include "timer.h"
#include "compressor.h"

#define	HTTPD_BIND_MAX 8
#define	HTTPD_MAX_STREAM_TIME (60 * 60 * 3)	/* seconds */

struct http_state {
	int	fd;
	uint16_t ts;
};

static volatile struct http_state * http_state;

size_t http_nstate;
const char * http_host;
const char * http_port;

static size_t
hpsjam_httpd_usage()
{
	size_t usage = 0;
	size_t x;

	for (x = 0; x < http_nstate; x++)
		usage += (http_state[x].fd != -1);
	return (usage);
}

static char *
hpsjam_httpd_read_line(FILE *io, char *linebuffer, size_t linelen)
{
	char buffer[2];
	size_t size = 0;

	if (fread(buffer, 1, 2, io) != 2)
		return (0);

	while (1) {
		if (buffer[0] == '\r' && buffer[1] == '\n')
			break;
		if (size == (linelen - 1))
			return (0);
		linebuffer[size++] = buffer[0];
		buffer[0] = buffer[1];
		if (fread(buffer + 1, 1, 1, io) != 1)
			return (0);
	}
	linebuffer[size++] = 0;

	return (linebuffer);
}

static int
hpsjam_http_generate_wav_header(FILE *io,
    uintmax_t r_start, uintmax_t r_end, bool is_partial)
{
	uint8_t buffer[256];
	uint8_t *ptr;
	uintmax_t dummy_len;
	uintmax_t delta;
	size_t mod;
	size_t len;
	size_t buflen;

	ptr = buffer;
	mod = HPSJAM_CHANNELS * HPSJAM_SAMPLE_BYTES;

	if (mod == 0 || sizeof(buffer) < (44 + mod - 1))
		return (-1);

	/* align to next sample */
	len = 44 + mod - 1;
	len -= len % mod;

	buflen = len;

	/* clear block */
	memset(ptr, 0, len);

	/* fill out data header */
	ptr[len - 8] = 'd';
	ptr[len - 7] = 'a';
	ptr[len - 6] = 't';
	ptr[len - 5] = 'a';

	/* magic for unspecified length */
	ptr[len - 4] = 0x00;
	ptr[len - 3] = 0xF0;
	ptr[len - 2] = 0xFF;
	ptr[len - 1] = 0x7F;

	/* fill out header */
	*ptr++ = 'R';
	*ptr++ = 'I';
	*ptr++ = 'F';
	*ptr++ = 'F';

	/* total chunk size - unknown */

	*ptr++ = 0;
	*ptr++ = 0;
	*ptr++ = 0;
	*ptr++ = 0;

	*ptr++ = 'W';
	*ptr++ = 'A';
	*ptr++ = 'V';
	*ptr++ = 'E';
	*ptr++ = 'f';
	*ptr++ = 'm';
	*ptr++ = 't';
	*ptr++ = ' ';

	/* make sure header fits in PCM block */
	len -= 28;

	*ptr++ = len;
	*ptr++ = len >> 8;
	*ptr++ = len >> 16;
	*ptr++ = len >> 24;

	/* audioformat = PCM */

	*ptr++ = 0x01;
	*ptr++ = 0x00;

	/* number of channels */

	len = HPSJAM_CHANNELS;

	*ptr++ = len;
	*ptr++ = len >> 8;

	/* sample rate */

	len = HPSJAM_SAMPLE_RATE;

	*ptr++ = len;
	*ptr++ = len >> 8;
	*ptr++ = len >> 16;
	*ptr++ = len >> 24;

	/* byte rate */

	len = HPSJAM_SAMPLE_RATE * HPSJAM_CHANNELS * HPSJAM_SAMPLE_BYTES;

	*ptr++ = len;
	*ptr++ = len >> 8;
	*ptr++ = len >> 16;
	*ptr++ = len >> 24;

	/* block align */

	len = HPSJAM_CHANNELS * HPSJAM_SAMPLE_BYTES;

	*ptr++ = len;
	*ptr++ = len >> 8;

	/* bits per sample */

	len = HPSJAM_SAMPLE_BYTES * 8;

	*ptr++ = len;
	*ptr++ = len >> 8;

	/* check if alignment is correct */
	if (r_start >= buflen && (r_start % mod) != 0)
		return (2);

	dummy_len = HPSJAM_SAMPLE_RATE * HPSJAM_CHANNELS * HPSJAM_SAMPLE_BYTES;
	dummy_len *= HTTPD_MAX_STREAM_TIME;

	/* fixup end */
	if (r_end >= dummy_len)
		r_end = dummy_len - 1;

	delta = r_end - r_start + 1;

	if (is_partial) {
		fprintf(io, "HTTP/1.1 206 Partial Content\r\n"
		    "Content-Type: audio/wav\r\n"
		    "Server: hpsjam/1.0\r\n"
		    "Cache-Control: no-cache, no-store\r\n"
		    "Expires: Mon, 26 Jul 1997 05:00:00 GMT\r\n"
		    "Connection: Close\r\n"
		    "Content-Range: bytes %ju-%ju/%ju\r\n"
		    "Content-Length: %ju\r\n"
		    "\r\n", r_start, r_end, dummy_len, delta);
	} else {
		fprintf(io, "HTTP/1.0 200 OK\r\n"
		    "Content-Type: audio/wav\r\n"
		    "Server: hpsjam/1.0\r\n"
		    "Cache-Control: no-cache, no-store\r\n"
		    "Expires: Mon, 26 Jul 1997 05:00:00 GMT\r\n"
		    "Connection: Close\r\n"
		    "Content-Length: %ju\r\n"
		    "\r\n", dummy_len);
	}

	/* check if we should insert a header */
	if (r_start < buflen) {
		buflen -= r_start;
		if (buflen > delta)
			buflen = delta;
		/* send data */
		if (fwrite(buffer + r_start, buflen, 1, io) != 1)
			return (-1);
		/* check if all data was read */
		if (buflen == delta)
			return (1);
	}
	return (0);
}

static void
hpsjam_httpd_handle_connection(int fd, const struct sockaddr_in *sa)
{
	char linebuffer[2048];
	uintmax_t r_start = 0;
	uintmax_t r_end = -1ULL;
	bool is_partial = false;
	char *line;
	FILE *io;
	size_t x;
	int page;

	io = fdopen(fd, "r+");
	if (io == 0)
		goto done;

	page = -1;

	/* dump HTTP request header */
	while (1) {
		line = hpsjam_httpd_read_line(io, linebuffer, sizeof(linebuffer));
		if (line == 0)
			goto done;
		if (line[0] == 0)
			break;
		if (page < 0 && (strstr(line, "GET / ") == line ||
		    strstr(line, "GET /index.html") == line)) {
			page = 0;
		} else if (page < 0 && strstr(line, "GET /stream.wav") == line) {
			page = 1;
		} else if (page < 0 && strstr(line, "GET /stream.m3u") == line) {
			page = 2;
		} else if (strstr(line, "Range: bytes=") == line &&
		    sscanf(line, "Range: bytes=%zu-%zu", &r_start, &r_end) >= 1) {
			is_partial = true;
		}
	}

	switch (page) {
	case 0:
		x = hpsjam_httpd_usage();

		fprintf(io, "HTTP/1.0 200 OK\r\n"
		    "Content-Type: text/html\r\n"
		    "Server: hpsjam/1.0\r\n"
		    "Cache-Control: no-cache, no-store\r\n"
		    "Expires: Mon, 26 Jul 1997 05:00:00 GMT\r\n"
		    "\r\n"
		    "<html><head><title>Welcome to live streaming</title>"
		    "<meta http-equiv=\"Cache-Control\" content=\"no-cache, no-store, must-revalidate\" />"
		    "<meta http-equiv=\"Pragma\" content=\"no-cache\" />"
		    "<meta http-equiv=\"Expires\" content=\"0\" />"
		    "</head>"
		    "<body>"
		    "<h1>Live HD stream</h1>"
		    "<br>"
		    "<br>"
		    "<h2>Alternative 1 (recommended)</h2>"
		    "<ol type=\"1\">"
		    "<li>Install <a href=\"https://www.videolan.org\">VideoLanClient (VLC)</a>, from App- or Play-store free of charge</li>"
		    "<li>Open VLC and select Network Stream</li>"
		    "<li>Enter, copy or share this network address to VLC: <a href=\"http://%s:%s/stream.m3u\">http://%s:%s/stream.m3u</a></li>"
		    "</ol>"
		    "<br>"
		    "<br>"
		    "<h2>Alternative 2 (on your own)</h2>"
		    "<br>"
		    "<br>"
		    "<audio id=\"audio\" controls=\"true\" src=\"stream.wav\" preload=\"none\"></audio>"
		    "<br>"
		    "<br>",
		    http_host, http_port,
		    http_host, http_port);

		if (x == http_nstate)
			fprintf(io, "<h2>There are currently no free slots (%zu active). Try again later!</h2>", x);
		else
			fprintf(io, "<h2>There are %zu free slots (%zu active)</h2>", http_nstate - x, x);

		fprintf(io, "</body></html>");
		break;
	case 1:
		for (x = 0; x < http_nstate; x++) {
			if (http_state[x].fd >= 0)
				continue;
			switch (hpsjam_http_generate_wav_header(io, r_start, r_end, is_partial)) {
				static const int enable = 1;

			case 0:
				fflush(io);
				fd = dup(fd);
				fclose(io);
				if (ioctl(fd, FIONBIO, &enable) != 0) {
					close(fd);
					return;
				}
				http_state[x].ts = hpsjam_ticks - 100;
				http_state[x].fd = fd;
				return;
			case 1:
				fclose(io);
				return;
			case 2:
				fprintf(io, "HTTP/1.1 416 Range Not Satisfiable\r\n"
				    "Server: hpsjam/1.0\r\n"
				    "\r\n");
				goto done;
			default:
				goto done;
			}
		}
		fprintf(io, "HTTP/1.0 503 Out of Resources\r\n"
		    "Server: hpsjam/1.0\r\n"
		    "\r\n");
		break;
	case 2:
		fprintf(io, "HTTP/1.0 200 OK\r\n"
		    "Content-Type: audio/mpegurl\r\n"
		    "Server: hpsjam/1.0\r\n"
		    "Cache-Control: no-cache, no-store\r\n"
		    "Expires: Mon, 26 Jul 1997 05:00:00 GMT\r\n"
		    "\r\n"
		    "http://%s:%s/stream.wav\r\n",
		    http_host, http_port);
		break;
	default:
		fprintf(io, "HTTP/1.0 404 Not Found\r\n"
		    "Content-Type: text/html\r\n"
		    "Server: hpsjam/1.0\r\n"
		    "\r\n"
		    "<html><head><title>Virtual OSS</title></head>"
		    "<body>"
		    "<h1>Invalid page requested! "
		    "<a HREF=\"index.html\">Click here to go back</a>.</h1><br>"
		    "</body>"
		    "</html>");
		break;
	}
done:
	if (io != 0)
		fclose(io);
	else
		close(fd);
}

static int
hpsjam_httpd_do_listen(const char *host, const char *port,
    struct pollfd *pfd, int num_sock, int buffer)
{
	static const struct timeval timeout = {.tv_sec = 1};
	struct addrinfo hints = {};
	struct addrinfo *res;
	struct addrinfo *res0;
	int error;
	int flag;
	int s;
	int ns = 0;

	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	if ((error = getaddrinfo(host, port, &hints, &res)))
		return (-1);

	res0 = res;

	do {
		if ((s = socket(res0->ai_family, res0->ai_socktype,
		    res0->ai_protocol)) < 0)
			continue;

		flag = 1;
		setsockopt(s, SOL_SOCKET, SO_REUSEPORT, &flag, (int)sizeof(flag));
		setsockopt(s, SOL_SOCKET, SO_SNDBUF, &buffer, (int)sizeof(buffer));
		setsockopt(s, SOL_SOCKET, SO_RCVBUF, &buffer, (int)sizeof(buffer));
		setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &timeout, (int)sizeof(timeout));
		setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &timeout, (int)sizeof(timeout));

		if (bind(s, res0->ai_addr, res0->ai_addrlen) == 0) {
			if (listen(s, http_nstate) == 0) {
				if (ns < num_sock) {
					pfd[ns++].fd = s;
					continue;
				}
				close(s);
				break;
			}
		}
		close(s);
	} while ((res0 = res0->ai_next) != 0);

	freeaddrinfo(res);

	return (ns);
}

static size_t
hpsjam_httpd_buflimit()
{
	/* don't buffer more than 250ms */
	return ((HPSJAM_SAMPLE_RATE / 4) * HPSJAM_CHANNELS * HPSJAM_SAMPLE_BYTES);
};

static void *
hpsjam_httpd_server(void *arg)
{
	const size_t bufferlimit = hpsjam_httpd_buflimit();
	const char *host = http_host;
	const char *port = http_port;
	struct sockaddr sa = {};
	struct pollfd fds[HTTPD_BIND_MAX] = {};
	int nfd;

	nfd = hpsjam_httpd_do_listen(host, port, fds, HTTPD_BIND_MAX, bufferlimit);
	if (nfd < 1)
		errx(1, "Could not bind to '%s' and '%s'", host, port);

	while (1) {
		int ns = nfd;
		int c;
		int f;

		for (c = 0; c != ns; c++) {
			fds[c].events = (POLLIN | POLLRDNORM | POLLRDBAND | POLLPRI |
			    POLLERR | POLLHUP | POLLNVAL);
			fds[c].revents = 0;
		}
		if (poll(fds, ns, -1) < 0)
			errx(1, "Polling failed");

		for (c = 0; c != ns; c++) {
			socklen_t socklen = sizeof(sa);

			if (fds[c].revents == 0)
				continue;
			f = accept(fds[c].fd, &sa, &socklen);
			if (f < 0)
				continue;
			hpsjam_httpd_handle_connection(f, (const struct sockaddr_in *)&sa);
		}
	}
	return (0);
}

void
hpsjam_httpd_streamer(const float *pleft, const float *pright, size_t samples)
{
	const size_t bufferlimit = hpsjam_httpd_buflimit();
	uint8_t buf[samples][HPSJAM_CHANNELS][HPSJAM_SAMPLE_BYTES];
	uint16_t ts;
	size_t x;

	for (x = 0; x != samples; x++) {
		static float http_in_peak;

		float left = pleft[x];
		float right = pright[x];

		hpsjam_stereo_compressor(HPSJAM_SAMPLE_RATE,
		    http_in_peak, left, right);

		const int32_t out[2] = {
		    (int32_t)(left * (1LL << 31)),
		    (int32_t)(right * (1LL << 31))
		};

		buf[x][0][0] = out[0] & 0xFF;
		buf[x][0][1] = (out[0] >> 8) & 0xFF;
		buf[x][0][2] = (out[0] >> 16) & 0xFF;
		buf[x][0][3] = (out[0] >> 24) & 0xFF;

		buf[x][1][0] = out[1] & 0xFF;
		buf[x][1][1] = (out[1] >> 8) & 0xFF;
		buf[x][1][2] = (out[1] >> 16) & 0xFF;
		buf[x][1][3] = (out[1] >> 24) & 0xFF;
	}

	/* get current ticks */
	ts = hpsjam_ticks;

	/* send HTTP data, if any */
	for (x = 0; x < http_nstate; x++) {
		int fd = http_state[x].fd;
		uint16_t delta = ts - http_state[x].ts;
		uint8_t tmp[1];
		int write_len;

		if (fd < 0) {
			/* do nothing */
		} else if (delta >= 8000) {
			/* no data for 8 seconds - terminate */
			http_state[x].fd = -1;
			close(fd);
		} else if (read(fd, tmp, sizeof(tmp)) != -1 || errno != EWOULDBLOCK) {
			http_state[x].fd = -1;
			close(fd);
#ifdef __linux__
		} else if (ioctl(fd, SIOCOUTQ, &write_len) < 0) {
			http_state[x].fd = -1;
			close(fd);
#else
		} else if (ioctl(fd, FIONWRITE, &write_len) < 0) {
			http_state[x].fd = -1;
			close(fd);
#endif
		} else if ((ssize_t)(bufferlimit - write_len) < (ssize_t)sizeof(buf)) {
			/* do nothing */
		} else if (write(fd, buf, sizeof(buf)) != (ssize_t)sizeof(buf)) {
			http_state[x].fd = -1;
			close(fd);
		} else {
			/* update timestamp */
			http_state[x].ts = ts;
		}
	}
}

void
hpsjam_httpd_start()
{
	pthread_t td;

	if (http_host == 0 || http_port == 0 || http_nstate == 0)
		return;

	http_state = new struct http_state [http_nstate];
	assert(http_state != 0);

	for (size_t x = 0; x != http_nstate; x++) {
		http_state[x].fd = -1;
		http_state[x].ts = 0;
	}

	if (pthread_create(&td, 0, hpsjam_httpd_server, 0))
		errx(1, "Could not create HTTP daemon thread");
}

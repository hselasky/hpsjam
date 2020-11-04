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

#include <QMutexLocker>

#include "hpsjam.h"

#include "peer.h"

#include "compressor.h"

void
hpsjam_peer_receive(const struct hpsjam_socket_address &src,
    const union hpsjam_frame &frame)
{
	if (hpsjam_num_server_peers == 0) {
		QMutexLocker locker(&hpsjam_locks[0]);

		if (hpsjam_client_peer->address == src)
			hpsjam_client_peer->input_pkt.receive(frame);
	} else {
		for (unsigned x = 0; x != hpsjam_num_server_peers; x++) {
			QMutexLocker locker(&hpsjam_locks[x]);

			if (hpsjam_server_peers[x].valid &&
			    hpsjam_server_peers[x].address == src) {
				hpsjam_server_peers[x].input_pkt.receive(frame);
				return;
			}
		}

		/* all new connections must start on a ping request */
		if (frame.start[0].valid(frame.end) == false ||
		    frame.start[0].type != HPSJAM_TYPE_PING_REQUEST)
			return;

		uint16_t packets;
		uint16_t time_ms;
		uint64_t passwd;

		/* check if ping message is valid */
		if (frame.start[0].getPing(packets, time_ms, passwd) == false)
			return;

		/* don't respond if password is invalid */
		if (hpsjam_server_passwd != 0 && passwd != hpsjam_server_passwd)
			return;

		/* create new connection */
		for (unsigned x = 0; x != hpsjam_num_server_peers; x++) {
			QMutexLocker locker(&hpsjam_locks[x]);

			if (hpsjam_server_peers[x].valid == false)
				continue;
			hpsjam_server_peers[x].valid = true;
			hpsjam_server_peers[x].address = src;
			hpsjam_server_peers[x].input_pkt.receive(frame);
			return;
		}
	}
}

void
hpsjam_client_peer :: sound_process(float *left, float *right, size_t samples)
{
	QMutexLocker locker(&hpsjam_locks[0]);

	float temp_l[samples];
	float temp_r[samples];

	/* Make a copy of input */
	memcpy(temp_l, left, sizeof(temp_l));
	memcpy(temp_r, left, sizeof(temp_r));

	/* Process bits */
	if (bits & HPSJAM_BIT_MUTE) {
		memset(left, 0, sizeof(left[0]) * samples);
		memset(right, 0, sizeof(right[0]) * samples);
	}

	/* Process equalizer */
	eq.doit(left, right, samples);

	/* Process panning */
	if (in_pan < 0.0f) {
		const float g[3] = { 1.0f + in_pan, 2.0f + in_pan, - in_pan };
		for (size_t x = 0; x != samples; x++) {
			float l = (left[x] * g[1] + right[x] * g[2]) / 2.0f;
			float r = right[x] * g[0];

			left[x] = l;
			right[x] = r;
		}
	} else if (in_pan > 0.0f) {
		const float g[3] = { 1.0f - in_pan, 2.0f - in_pan, in_pan };
		for (size_t x = 0; x != samples; x++) {
			float l = left[x] * g[0];
			float r = (right[x] * g[1] + left[x] * g[2]) / 2.0f;

			left[x] = l;
			right[x] = r;
		}
	}

	/* Process gain */
	if (in_gain < 1.0f) {
		for (size_t x = 0; x != samples; x++) {
			left[x] *= in_gain;
			right[x] *= in_gain;
		}
	}

	/* Process compressor */
	for (size_t x = 0; x != samples; x++) {
		hpsjam_stereo_compressor(HPSJAM_SAMPLE_RATE,
		    in_peak, left[x], right[x]);
	}

	in_audio[0].addSamples(left, samples);
	in_audio[1].addSamples(right, samples);

	out_audio[0].remSamples(left, samples);
	out_audio[1].remSamples(right, samples);

	/* Process bits */
	if (bits & HPSJAM_BIT_SOLO) {
		memset(left, 0, sizeof(left[0]) * samples);
		memset(right, 0, sizeof(right[0]) * samples);
	}

	/* Add monitor */
	if (mon_gain != 0.0f) {
		for (size_t x = 0; x != samples; x++) {
			left[x] += temp_l[x] * mon_gain;
			right[x] += temp_r[x] * mon_gain;
		}
	}

	/* Process final compressor */
	for (size_t x = 0; x != samples; x++) {
		hpsjam_stereo_compressor(HPSJAM_SAMPLE_RATE,
		    out_peak, left[x], right[x]);
	}
}

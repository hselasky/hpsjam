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

#include "timer.h"

Q_DECL_EXPORT void
hpsjam_peer_receive(const struct hpsjam_socket_address &src,
    const union hpsjam_frame &frame)
{
	if (hpsjam_num_server_peers == 0) {
		QMutexLocker locker(&hpsjam_client_peer->lock);

		if (hpsjam_client_peer->address == src)
			hpsjam_client_peer->input_pkt.receive(frame);
	} else {
		const struct hpsjam_packet *ptr;

		for (unsigned x = hpsjam_num_server_peers; x--; ) {
			QMutexLocker locker(&hpsjam_server_peers[x].lock);

			if (hpsjam_server_peers[x].valid &&
			    hpsjam_server_peers[x].address == src) {
				hpsjam_server_peers[x].input_pkt.receive(frame);
				return;
			}
		}

		/* all new connections must start on a ping request */
		for (ptr = frame.start; ptr->valid(frame.end); ptr = ptr->next()) {
			if (ptr->type == HPSJAM_TYPE_PING_REQUEST)
				break;
		}

		/* check if we have a valid chunk */
		if (ptr->valid(frame.end) == false)
			return;

		uint16_t packets;
		uint16_t time_ms;
		uint64_t passwd;

		/* check if ping message is valid */
		if (ptr->getPing(packets, time_ms, passwd) == false)
			return;

		/* don't respond if password is invalid */
		if (hpsjam_server_passwd != 0 && passwd != hpsjam_server_passwd)
			return;

		/* create new connection, if any */
		for (unsigned x = hpsjam_num_server_peers; x--; ) {
			QMutexLocker locker(&hpsjam_server_peers[x].lock);

			if (hpsjam_server_peers[x].valid == true)
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
	QMutexLocker locker(&lock);

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

	in_level[0].addSamples(left, samples);
	in_level[1].addSamples(right, samples);

	out_audio[0].remSamples(left, samples);
	out_audio[1].remSamples(right, samples);

	/* Process bits */
	if (bits & HPSJAM_BIT_SOLO) {
		memset(left, 0, sizeof(left[0]) * samples);
		memset(right, 0, sizeof(right[0]) * samples);
	}

	/* Add monitor */
	if (mon_gain != 0.0f) {
		float gain = (bits & HPSJAM_BIT_INVERT) ? - mon_gain : mon_gain;

		/* Process panning */
		if (mon_pan < 0.0f) {
			const float g[3] = { 1.0f + mon_pan, 2.0f + mon_pan, - mon_pan };
			for (size_t x = 0; x != samples; x++) {
				float l = (temp_l[x] * g[1] + temp_r[x] * g[2]) / 2.0f;
				float r = temp_r[x] * g[0];

				left[x] += l * gain;
				right[x] += r * gain;
			}
		} else if (mon_pan > 0.0f) {
			const float g[3] = { 1.0f - mon_pan, 2.0f - mon_pan, mon_pan };
			for (size_t x = 0; x != samples; x++) {
				float l = temp_l[x] * g[0];
				float r = (temp_r[x] * g[1] + temp_l[x] * g[2]) / 2.0f;

				left[x] += l * gain;
				right[x] += r * gain;
			}
		} else {
			for (size_t x = 0; x != samples; x++) {
				left[x] += temp_l[x] * gain;
				right[x] += temp_r[x] * gain;
			}
		}
	}

	/* Process final compressor */
	for (size_t x = 0; x != samples; x++) {
		hpsjam_stereo_compressor(HPSJAM_SAMPLE_RATE,
		    out_peak, left[x], right[x]);
	}
}

void
hpsjam_server_peer :: handle_pending_watchdog()
{
	QMutexLocker locker(&lock);

	if (address.valid() && output_pkt.empty()) {
		struct hpsjam_packet_entry *pkt = new struct hpsjam_packet_entry;
		pkt->packet.setPing(0, hpsjam_ticks, 0);
		pkt->packet.type = HPSJAM_TYPE_PING_REQUEST;
		pkt->insert_tail(&output_pkt.head);
	}
}

void
hpsjam_server_peer :: handle_pending_timeout()
{
	QMutexLocker locker(&lock);

	init();
}

void
hpsjam_server_peer :: audio_export()
{
	const union hpsjam_frame *pkt;
	const struct hpsjam_packet *ptr;
	struct hpsjam_packet_entry *pres;
	float temp[HPSJAM_MAX_PKT];
	size_t num;

	input_pkt.recovery();

	while ((pkt = input_pkt.first_pkt())) {
		for (ptr = pkt->start; ptr->valid(pkt->end); ptr = ptr->next()) {
			switch (ptr->type) {
			case HPSJAM_TYPE_AUDIO_8_BIT_1CH:
				num = ptr->get8Bit1ChSample(temp);
				assert(num <= HPSJAM_MAX_PKT);
				in_audio[0].addSamples(temp, num);
				in_audio[1].addSamples(temp, num);
				break;
			case HPSJAM_TYPE_AUDIO_16_BIT_1CH:
				num = ptr->get16Bit1ChSample(temp);
				assert(num <= HPSJAM_MAX_PKT);
				in_audio[0].addSamples(temp, num);
				in_audio[1].addSamples(temp, num);
				break;
			case HPSJAM_TYPE_AUDIO_24_BIT_1CH:
				num = ptr->get24Bit1ChSample(temp);
				assert(num <= HPSJAM_MAX_PKT);
				in_audio[0].addSamples(temp, num);
				in_audio[1].addSamples(temp, num);
				break;
			case HPSJAM_TYPE_AUDIO_32_BIT_1CH:
				num = ptr->get32Bit1ChSample(temp);
				assert(num <= HPSJAM_MAX_PKT);
				in_audio[0].addSamples(temp, num);
				in_audio[1].addSamples(temp, num);
				break;
			case HPSJAM_TYPE_AUDIO_8_BIT_2CH:
				num = ptr->get8Bit2ChSample(temp, temp + (HPSJAM_MAX_PKT / 2));
				assert(num <= (HPSJAM_MAX_PKT / 2));
				in_audio[0].addSamples(temp, num);
				in_audio[1].addSamples(temp + (HPSJAM_MAX_PKT / 2), num);
				break;
			case HPSJAM_TYPE_AUDIO_16_BIT_2CH:
				num = ptr->get16Bit2ChSample(temp, temp + (HPSJAM_MAX_PKT / 2));
				assert(num <= (HPSJAM_MAX_PKT / 2));
				in_audio[0].addSamples(temp, num);
				in_audio[1].addSamples(temp + (HPSJAM_MAX_PKT / 2), num);
				break;
			case HPSJAM_TYPE_AUDIO_24_BIT_2CH:
				num = ptr->get24Bit2ChSample(temp, temp + (HPSJAM_MAX_PKT / 2));
				assert(num <= (HPSJAM_MAX_PKT / 2));
				in_audio[0].addSamples(temp, num);
				in_audio[1].addSamples(temp + (HPSJAM_MAX_PKT / 2), num);
				break;
			case HPSJAM_TYPE_AUDIO_32_BIT_2CH:
				num = ptr->get32Bit2ChSample(temp, temp + (HPSJAM_MAX_PKT / 2));
				assert(num <= (HPSJAM_MAX_PKT / 2));
				in_audio[0].addSamples(temp, num);
				in_audio[1].addSamples(temp + (HPSJAM_MAX_PKT / 2), num);
				break;
			case HPSJAM_TYPE_AUDIO_32_BIT_2CH + 1 ... HPSJAM_TYPE_AUDIO_MAX:
				/* for the future */
				break;
			default:
				/* check if other side received packet */
				if (ptr->getPeerSeqNo() == output_pkt.pend_seqno)
					output_pkt.advance();
				/* check if sequence number matches */
				if (ptr->getLocalSeqNo() != output_pkt.peer_seqno)
					continue;
				/* advance expected sequence number */
				output_pkt.peer_seqno++;

				switch (ptr->type) {
				uint16_t packets;
				uint16_t time_ms;
				uint64_t passwd;

				case HPSJAM_TYPE_CONFIGURE_REQUEST:
					if (ptr->getConfigure(output_fmt))
						break;
					output_fmt = HPSJAM_TYPE_END;
					break;
				case HPSJAM_TYPE_PING_REQUEST:
					if (ptr->getPing(packets, time_ms, passwd) &&
					    output_pkt.find(HPSJAM_TYPE_PING_REPLY) == 0) {
						pres = new struct hpsjam_packet_entry;
						pres->packet.setPing(0, time_ms, 0);
						pres->packet.type = HPSJAM_TYPE_PING_REPLY;
						pres->insert_tail(&output_pkt.head);
					}
					break;
				case HPSJAM_TYPE_PING_REPLY:
					break;
				case HPSJAM_TYPE_FADER_GAIN_REQUEST:
				case HPSJAM_TYPE_FADER_GAIN_REPLY:
				case HPSJAM_TYPE_FADER_PAN_REQUEST:
				case HPSJAM_TYPE_FADER_PAN_REPLY:
				case HPSJAM_TYPE_FADER_ICON_REQUEST:
				case HPSJAM_TYPE_FADER_ICON_REPLY:
				case HPSJAM_TYPE_FADER_NAME_REQUEST:
				case HPSJAM_TYPE_FADER_NAME_REPLY:
				case HPSJAM_TYPE_LYRICS_REQUEST:
				case HPSJAM_TYPE_LYRICS_REPLY:
				case HPSJAM_TYPE_CHAT_REQUEST:
				case HPSJAM_TYPE_CHAT_REPLY:
				default:
					break;
				}

				if (output_pkt.empty()) {
					pres = new struct hpsjam_packet_entry;
					pres->packet.setPing(0, hpsjam_ticks, 0);
					pres->packet.type = HPSJAM_TYPE_PING_REQUEST;
					pres->insert_tail(&output_pkt.head);
				}
			}
		}
	}

	/* extract samples for this tick */
	in_audio[0].remSamples(tmp_audio[0], HPSJAM_SAMPLE_RATE / 1000);
	in_audio[1].remSamples(tmp_audio[1], HPSJAM_SAMPLE_RATE / 1000);

	/* clear output audio */
	memset(out_audio, 0, sizeof(out_audio));
}

void
hpsjam_server_peer :: audio_import()
{
	struct hpsjam_packet_entry entry = {};

	/* compute levels */
	out_level[0].addSamples(out_audio[0], HPSJAM_SAMPLE_RATE / 1000);
	out_level[1].addSamples(out_audio[1], HPSJAM_SAMPLE_RATE / 1000);

	/* run compressor before sending audio */
	switch (output_fmt) {
	case HPSJAM_TYPE_AUDIO_8_BIT_1CH:
	case HPSJAM_TYPE_AUDIO_16_BIT_1CH:
	case HPSJAM_TYPE_AUDIO_24_BIT_1CH:
	case HPSJAM_TYPE_AUDIO_32_BIT_1CH:
		for (unsigned int x = 0; x != (HPSJAM_SAMPLE_RATE / 1000); x++) {
			out_audio[0][x] = (out_audio[0][x] + out_audio[1][x]) / 2.0f;

			hpsjam_mono_compressor(HPSJAM_SAMPLE_RATE,
			    out_peak, out_audio[0][x]);
		}
		break;
	case HPSJAM_TYPE_AUDIO_8_BIT_2CH:
	case HPSJAM_TYPE_AUDIO_16_BIT_2CH:
	case HPSJAM_TYPE_AUDIO_24_BIT_2CH:
	case HPSJAM_TYPE_AUDIO_32_BIT_2CH:
		for (unsigned int x = 0; x != (HPSJAM_SAMPLE_RATE / 1000); x++) {
			hpsjam_stereo_compressor(HPSJAM_SAMPLE_RATE,
			    out_peak, out_audio[0][x], out_audio[1][x]);
		}
		break;
	default:
		break;
	}

	switch (output_fmt) {
	case HPSJAM_TYPE_AUDIO_8_BIT_1CH:
		entry.packet.put8Bit1ChSample(out_audio[0], HPSJAM_SAMPLE_RATE / 1000);
		output_pkt.append(entry);
		break;
	case HPSJAM_TYPE_AUDIO_16_BIT_1CH:
		entry.packet.put16Bit1ChSample(out_audio[0], HPSJAM_SAMPLE_RATE / 1000);
		output_pkt.append(entry);
		break;
	case HPSJAM_TYPE_AUDIO_24_BIT_1CH:
		entry.packet.put24Bit1ChSample(out_audio[0], HPSJAM_SAMPLE_RATE / 1000);
		output_pkt.append(entry);
		break;
	case HPSJAM_TYPE_AUDIO_32_BIT_1CH:
		entry.packet.put8Bit1ChSample(out_audio[0], HPSJAM_SAMPLE_RATE / 1000);
		output_pkt.append(entry);
		break;
	case HPSJAM_TYPE_AUDIO_8_BIT_2CH:
		entry.packet.put8Bit2ChSample(out_audio[0], out_audio[1], HPSJAM_SAMPLE_RATE / 1000);
		output_pkt.append(entry);
		break;
	case HPSJAM_TYPE_AUDIO_16_BIT_2CH:
		entry.packet.put16Bit2ChSample(out_audio[0], out_audio[1], HPSJAM_SAMPLE_RATE / 1000);
		output_pkt.append(entry);
		break;
	case HPSJAM_TYPE_AUDIO_24_BIT_2CH:
		entry.packet.put24Bit2ChSample(out_audio[0], out_audio[1], HPSJAM_SAMPLE_RATE / 1000);
		output_pkt.append(entry);
		break;
	case HPSJAM_TYPE_AUDIO_32_BIT_2CH:
		entry.packet.put32Bit2ChSample(out_audio[0], out_audio[1], HPSJAM_SAMPLE_RATE / 1000);
		output_pkt.append(entry);
		break;
	default:
		break;
	}

	/* send a packet */
	output_pkt.send(address);
}

Q_DECL_EXPORT void
hpsjam_server_tick()
{
	/* get audio */
	for (unsigned x = 0; x != hpsjam_num_server_peers; x++) {
		QMutexLocker locker(&hpsjam_server_peers[x].lock);

		if (hpsjam_server_peers[x].valid == false) {
			memset(hpsjam_server_peers[x].tmp_audio, 0,
			       sizeof(hpsjam_server_peers[x].tmp_audio));
			continue;
		}
		hpsjam_server_peers[x].audio_export();
	}

	/* mix everything */
	for (unsigned x = 0; x != hpsjam_num_server_peers; x++) {
		QMutexLocker locker(&hpsjam_server_peers[x].lock);

		if (hpsjam_server_peers[x].valid == false)
			continue;

		for (unsigned y = 0; y != hpsjam_num_server_peers; y++) {
			if (hpsjam_server_peers[x].bits[y] & HPSJAM_BIT_SOLO)
				goto do_solo;
		}

		for (unsigned y = 0; y != hpsjam_num_server_peers; y++) {
			if (hpsjam_server_peers[x].bits[y] & HPSJAM_BIT_MUTE)
				continue;
			if (hpsjam_server_peers[x].bits[y] & HPSJAM_BIT_INVERT) {
				for (unsigned z = 0; z != (HPSJAM_SAMPLE_RATE / 1000); z++) {
					hpsjam_server_peers[x].out_audio[0][z] -= hpsjam_server_peers[y].tmp_audio[0][z];
					hpsjam_server_peers[x].out_audio[1][z] -= hpsjam_server_peers[y].tmp_audio[1][z];
				}
			} else {
				for (unsigned z = 0; z != (HPSJAM_SAMPLE_RATE / 1000); z++) {
					hpsjam_server_peers[x].out_audio[0][z] += hpsjam_server_peers[y].tmp_audio[0][z];
					hpsjam_server_peers[x].out_audio[1][z] += hpsjam_server_peers[y].tmp_audio[1][z];
				}
			}
		}
		continue;
	do_solo:
		for (unsigned y = 0; y != hpsjam_num_server_peers; y++) {
			if (~hpsjam_server_peers[x].bits[y] & HPSJAM_BIT_SOLO)
				continue;
			if (hpsjam_server_peers[x].bits[y] & HPSJAM_BIT_INVERT) {
				for (unsigned z = 0; z != (HPSJAM_SAMPLE_RATE / 1000); z++) {
					hpsjam_server_peers[x].out_audio[0][z] -= hpsjam_server_peers[y].tmp_audio[0][z];
					hpsjam_server_peers[x].out_audio[1][z] -= hpsjam_server_peers[y].tmp_audio[1][z];
				}
			} else {
				for (unsigned z = 0; z != (HPSJAM_SAMPLE_RATE / 1000); z++) {
					hpsjam_server_peers[x].out_audio[0][z] += hpsjam_server_peers[y].tmp_audio[0][z];
					hpsjam_server_peers[x].out_audio[1][z] += hpsjam_server_peers[y].tmp_audio[1][z];
				}
			}
		}
	}

	/* send audio */
	for (unsigned x = 0; x != hpsjam_num_server_peers; x++) {
		QMutexLocker locker(&hpsjam_server_peers[x].lock);

		if (hpsjam_server_peers[x].valid == false)
			continue;
		hpsjam_server_peers[x].audio_import();
	}
}

void
hpsjam_client_peer :: handle_pending_watchdog()
{
	QMutexLocker locker(&lock);

	if (address.valid() && output_pkt.empty()) {
		struct hpsjam_packet_entry *pkt = new struct hpsjam_packet_entry;
		pkt->packet.setPing(0, hpsjam_ticks, 0);
		pkt->packet.type = HPSJAM_TYPE_PING_REQUEST;
		pkt->insert_tail(&output_pkt.head);
	}
}

void
hpsjam_client_peer :: tick()
{
	QMutexLocker locker(&lock);

	struct hpsjam_packet_entry entry = {};
	const union hpsjam_frame *pkt;
	const struct hpsjam_packet *ptr;
	struct hpsjam_packet_entry *pres;
	union {
		float temp[HPSJAM_MAX_PKT];
		float audio[2][HPSJAM_SAMPLE_RATE / 1000];
	};
	size_t num;

	input_pkt.recovery();

	while ((pkt = input_pkt.first_pkt())) {
		for (ptr = pkt->start; ptr->valid(pkt->end); ptr = ptr->next()) {
			switch (ptr->type) {
			case HPSJAM_TYPE_AUDIO_8_BIT_1CH:
				num = ptr->get8Bit1ChSample(temp);
				assert(num <= HPSJAM_MAX_PKT);
				out_audio[0].addSamples(temp, num);
				out_audio[1].addSamples(temp, num);
				break;
			case HPSJAM_TYPE_AUDIO_16_BIT_1CH:
				num = ptr->get16Bit1ChSample(temp);
				assert(num <= HPSJAM_MAX_PKT);
				out_audio[0].addSamples(temp, num);
				out_audio[1].addSamples(temp, num);
				break;
			case HPSJAM_TYPE_AUDIO_24_BIT_1CH:
				num = ptr->get24Bit1ChSample(temp);
				assert(num <= HPSJAM_MAX_PKT);
				out_audio[0].addSamples(temp, num);
				out_audio[1].addSamples(temp, num);
				break;
			case HPSJAM_TYPE_AUDIO_32_BIT_1CH:
				num = ptr->get32Bit1ChSample(temp);
				assert(num <= HPSJAM_MAX_PKT);
				out_audio[0].addSamples(temp, num);
				out_audio[1].addSamples(temp, num);
				break;
			case HPSJAM_TYPE_AUDIO_8_BIT_2CH:
				num = ptr->get8Bit2ChSample(temp, temp + (HPSJAM_MAX_PKT / 2));
				assert(num <= (HPSJAM_MAX_PKT / 2));
				out_audio[0].addSamples(temp, num);
				out_audio[1].addSamples(temp + (HPSJAM_MAX_PKT / 2), num);
				break;
			case HPSJAM_TYPE_AUDIO_16_BIT_2CH:
				num = ptr->get16Bit2ChSample(temp, temp + (HPSJAM_MAX_PKT / 2));
				assert(num <= (HPSJAM_MAX_PKT / 2));
				out_audio[0].addSamples(temp, num);
				out_audio[1].addSamples(temp + (HPSJAM_MAX_PKT / 2), num);
				break;
			case HPSJAM_TYPE_AUDIO_24_BIT_2CH:
				num = ptr->get24Bit2ChSample(temp, temp + (HPSJAM_MAX_PKT / 2));
				assert(num <= (HPSJAM_MAX_PKT / 2));
				out_audio[0].addSamples(temp, num);
				out_audio[1].addSamples(temp + (HPSJAM_MAX_PKT / 2), num);
				break;
			case HPSJAM_TYPE_AUDIO_32_BIT_2CH:
				num = ptr->get32Bit2ChSample(temp, temp + (HPSJAM_MAX_PKT / 2));
				assert(num <= (HPSJAM_MAX_PKT / 2));
				out_audio[0].addSamples(temp, num);
				out_audio[1].addSamples(temp + (HPSJAM_MAX_PKT / 2), num);
				break;
			case HPSJAM_TYPE_AUDIO_32_BIT_2CH + 1 ... HPSJAM_TYPE_AUDIO_MAX:
				/* for the future */
				break;
			default:
				/* check if other side received packet */
				if (ptr->getPeerSeqNo() == output_pkt.pend_seqno)
					output_pkt.advance();
				/* check if sequence number matches */
				if (ptr->getLocalSeqNo() != output_pkt.peer_seqno)
					continue;
				/* advance expected sequence number */
				output_pkt.peer_seqno++;

				switch (ptr->type) {
				uint16_t packets;
				uint16_t time_ms;
				uint64_t passwd;

				case HPSJAM_TYPE_CONFIGURE_REQUEST:
					break;
				case HPSJAM_TYPE_PING_REQUEST:
					if (ptr->getPing(packets, time_ms, passwd) &&
					    output_pkt.find(HPSJAM_TYPE_PING_REPLY) == 0) {
						pres = new struct hpsjam_packet_entry;
						pres->packet.setPing(0, time_ms, 0);
						pres->packet.type = HPSJAM_TYPE_PING_REPLY;
						pres->insert_tail(&output_pkt.head);
					}
					break;
				case HPSJAM_TYPE_PING_REPLY:
					break;
				case HPSJAM_TYPE_FADER_GAIN_REQUEST:
				case HPSJAM_TYPE_FADER_GAIN_REPLY:
				case HPSJAM_TYPE_FADER_PAN_REQUEST:
				case HPSJAM_TYPE_FADER_PAN_REPLY:
				case HPSJAM_TYPE_FADER_ICON_REQUEST:
				case HPSJAM_TYPE_FADER_ICON_REPLY:
				case HPSJAM_TYPE_FADER_NAME_REQUEST:
				case HPSJAM_TYPE_FADER_NAME_REPLY:
				case HPSJAM_TYPE_LYRICS_REQUEST:
				case HPSJAM_TYPE_LYRICS_REPLY:
				case HPSJAM_TYPE_CHAT_REQUEST:
				case HPSJAM_TYPE_CHAT_REPLY:
				default:
					break;
				}

				if (output_pkt.empty()) {
					pres = new struct hpsjam_packet_entry;
					pres->packet.setPing(0, hpsjam_ticks, 0);
					pres->packet.type = HPSJAM_TYPE_PING_REQUEST;
					pres->insert_tail(&output_pkt.head);
				}
			}
		}
	}

	/* extract samples for this tick */
	in_audio[0].remSamples(audio[0], HPSJAM_SAMPLE_RATE / 1000);
	in_audio[1].remSamples(audio[1], HPSJAM_SAMPLE_RATE / 1000);

	switch (out_format) {
	case HPSJAM_TYPE_AUDIO_8_BIT_1CH:
	case HPSJAM_TYPE_AUDIO_16_BIT_1CH:
	case HPSJAM_TYPE_AUDIO_24_BIT_1CH:
	case HPSJAM_TYPE_AUDIO_32_BIT_1CH:
		for (unsigned int x = 0; x != (HPSJAM_SAMPLE_RATE / 1000); x++)
			audio[0][x] = (audio[0][x] + audio[1][x]) / 2.0f;
		break;
	default:
		break;
	}

	switch (out_format) {
	case HPSJAM_TYPE_AUDIO_8_BIT_1CH:
		entry.packet.put8Bit1ChSample(audio[0], HPSJAM_SAMPLE_RATE / 1000);
		output_pkt.append(entry);
		break;
	case HPSJAM_TYPE_AUDIO_16_BIT_1CH:
		entry.packet.put16Bit1ChSample(audio[0], HPSJAM_SAMPLE_RATE / 1000);
		output_pkt.append(entry);
		break;
	case HPSJAM_TYPE_AUDIO_24_BIT_1CH:
		entry.packet.put24Bit1ChSample(audio[0], HPSJAM_SAMPLE_RATE / 1000);
		output_pkt.append(entry);
		break;
	case HPSJAM_TYPE_AUDIO_32_BIT_1CH:
		entry.packet.put8Bit1ChSample(audio[0], HPSJAM_SAMPLE_RATE / 1000);
		output_pkt.append(entry);
		break;
	case HPSJAM_TYPE_AUDIO_8_BIT_2CH:
		entry.packet.put8Bit2ChSample(audio[0], audio[1], HPSJAM_SAMPLE_RATE / 1000);
		output_pkt.append(entry);
		break;
	case HPSJAM_TYPE_AUDIO_16_BIT_2CH:
		entry.packet.put16Bit2ChSample(audio[0], audio[1], HPSJAM_SAMPLE_RATE / 1000);
		output_pkt.append(entry);
		break;
	case HPSJAM_TYPE_AUDIO_24_BIT_2CH:
		entry.packet.put24Bit2ChSample(audio[0], audio[1], HPSJAM_SAMPLE_RATE / 1000);
		output_pkt.append(entry);
		break;
	case HPSJAM_TYPE_AUDIO_32_BIT_2CH:
		entry.packet.put32Bit2ChSample(audio[0], audio[1], HPSJAM_SAMPLE_RATE / 1000);
		output_pkt.append(entry);
		break;
	default:
		break;
	}

	/* send a packet */
	output_pkt.send(address);
}

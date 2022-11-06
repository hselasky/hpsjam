/*-
 * Copyright (c) 2020-2022 Hans Petter Selasky. All rights reserved.
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

#include <QTextStream>
#include <QMutexLocker>
#include <QFile>

#include "hpsjam.h"
#include "peer.h"
#include "compressor.h"
#include "clientdlg.h"
#include "chatdlg.h"
#include "lyricsdlg.h"
#include "httpd.h"

#include "timer.h"

Q_DECL_EXPORT void
hpsjam_peer_receive(const struct hpsjam_socket_address &src,
    const struct hpsjam_socket_address &dst, const union hpsjam_frame &frame)
{
	if (hpsjam_num_server_peers == 0) {
		QMutexLocker locker(&hpsjam_client_peer->lock);

		if (hpsjam_client_peer->address[0].valid()) {
			for (unsigned i = 0; i != HPSJAM_PORTS_MAX; i++) {
				if (hpsjam_client_peer->address[i] == src) {
					hpsjam_client_peer->input_pkt.receive(frame);
					break;
				}
			}
		}
	} else {
		const struct hpsjam_packet *ptr;

		for (unsigned x = hpsjam_num_server_peers; x--; ) {
			class hpsjam_server_peer &peer = hpsjam_server_peers[x];

			QMutexLocker locker(&peer.lock);

			if (peer.valid && peer.address[0] == src) {
				peer.input_pkt.receive(frame);
				return;
			}
		}

		/*
		 * All new connections must start on a ping request
		 * having sequence number zero:
		 */
		for (ptr = frame.start; ptr->valid(frame.end); ptr = ptr->next()) {
			if (ptr->type == HPSJAM_TYPE_PING_REQUEST &&
			    ptr->sequence[0] == 0 && ptr->sequence[1] == 0)
				break;
		}

		/* check if we have a valid chunk */
		if (ptr->valid(frame.end) == false)
			return;

		uint16_t packets;
		uint16_t time_ms;
		uint32_t features;
		uint64_t passwd;

		/* check if ping message is valid */
		if (ptr->getPing(packets, time_ms, passwd, features) == false)
			return;

		/* don't respond if password is invalid */
		if (hpsjam_server_passwd != 0 && passwd != hpsjam_server_passwd) {
			if (hpsjam_mixer_passwd == 0 || passwd != hpsjam_mixer_passwd)
				return;
		}

		/* create new connection, if any */
		for (unsigned x = hpsjam_num_server_peers; x--; ) {
			class hpsjam_server_peer &peer = hpsjam_server_peers[x];
			QMutexLocker peer_locker(&peer.lock);

			if (peer.valid == true)
				continue;

			peer.allow_mixer_access =
			    (hpsjam_mixer_passwd == 0 || hpsjam_mixer_passwd == passwd);
			peer.valid = true;
			for (unsigned i = 0; i != HPSJAM_PORTS_MAX; i++) {
				peer.address[i] = src;
				switch (src.v4.sin_family) {
				case AF_INET:
					peer.address[i].fd = hpsjam_v4[i].fd;
					break;
				case AF_INET6:
					peer.address[i].fd = hpsjam_v6[i].fd;
					break;
				default:
					break;
				}
			}
			peer.gain = 1.0f;
			peer.pan = 0.0f;
			delete [] peer.eq_data;
			peer.eq_data = 0;
			peer.eq_size = 0;
			peer.input_pkt.receive(frame);
			peer.send_welcome_message();
			peer.send_mixer_parameters();

			/* drop lock */
			peer_locker.unlock();

			/* reset bits for this client */
			for (unsigned y = hpsjam_num_server_peers; y--; ) {
				class hpsjam_server_peer &other = hpsjam_server_peers[y];
				QMutexLocker other_locker(&other.lock);
				other.bits[x] = 0;
			}
			return;
		}
	}
}

static void
hpsjam_server_broadcast(const struct hpsjam_packet_entry &entry,
    class hpsjam_server_peer *except = 0, bool single = false)
{
	struct hpsjam_packet_entry *ptr;

	for (unsigned x = 0; x != hpsjam_num_server_peers; x++) {
		if (hpsjam_server_peers + x == except)
			continue;

		class hpsjam_server_peer &peer = hpsjam_server_peers[x];
		QMutexLocker locker(&peer.lock);

		if (peer.valid == false)
			continue;

		/* check if a level packet is already pending */
		if (single && peer.output_pkt.find(entry.packet.type))
			continue;
		/* duplicate packet */
		ptr = new struct hpsjam_packet_entry;
		*ptr = entry;
		ptr->insert_tail(&peer.output_pkt.head);
	}
}

static const uint8_t hpsjam_midi_cmd_to_len[16] = {
	0,			/* reserved */
	0,			/* reserved */
	2,			/* bytes */
	3,			/* bytes */
	3,			/* bytes */
	1,			/* bytes */
	2,			/* bytes */
	3,			/* bytes */
	3,			/* bytes */
	3,			/* bytes */
	3,			/* bytes */
	3,			/* bytes */
	2,			/* bytes */
	2,			/* bytes */
	3,			/* bytes */
	1,			/* bytes */
};

/*
 * The following statemachine, that converts MIDI commands to
 * USB MIDI packets, derives from Linux's usbmidi.c, which
 * was written by "Clemens Ladisch":
 *
 * Returns:
 *    0: No command
 * Else: Command is complete
 */
static uint8_t
hpsjam_midi_convert_to_usb(struct hpsjam_midi_parse *parser, uint8_t cn, uint8_t b)
{
	uint8_t p0 = (cn << 4);

	if (b >= 0xf8) {
		parser->temp_0[0] = p0 | 0x0f;
		parser->temp_0[1] = b;
		parser->temp_0[2] = 0;
		parser->temp_0[3] = 0;
		parser->temp_cmd = parser->temp_0;
		return (1);

	} else if (b >= 0xf0) {
		switch (b) {
		case 0xf0:		/* system exclusive begin */
			parser->temp_1[1] = b;
			parser->state = HPSJAM_MIDI_ST_SYSEX_1;
			break;
		case 0xf1:		/* MIDI time code */
		case 0xf3:		/* song select */
			parser->temp_1[1] = b;
			parser->state = HPSJAM_MIDI_ST_1PARAM;
			break;
		case 0xf2:		/* song position pointer */
			parser->temp_1[1] = b;
			parser->state = HPSJAM_MIDI_ST_2PARAM_1;
			break;
		case 0xf4:		/* unknown */
		case 0xf5:		/* unknown */
			parser->state = HPSJAM_MIDI_ST_UNKNOWN;
			break;
		case 0xf6:		/* tune request */
			parser->temp_1[0] = p0 | 0x05;
			parser->temp_1[1] = 0xf6;
			parser->temp_1[2] = 0;
			parser->temp_1[3] = 0;
			parser->temp_cmd = parser->temp_1;
			parser->state = HPSJAM_MIDI_ST_UNKNOWN;
			return (1);

		case 0xf7:		/* system exclusive end */
			switch (parser->state) {
			case HPSJAM_MIDI_ST_SYSEX_0:
				parser->temp_1[0] = p0 | 0x05;
				parser->temp_1[1] = 0xf7;
				parser->temp_1[2] = 0;
				parser->temp_1[3] = 0;
				parser->temp_cmd = parser->temp_1;
				parser->state = HPSJAM_MIDI_ST_UNKNOWN;
				return (1);
			case HPSJAM_MIDI_ST_SYSEX_1:
				parser->temp_1[0] = p0 | 0x06;
				parser->temp_1[2] = 0xf7;
				parser->temp_1[3] = 0;
				parser->temp_cmd = parser->temp_1;
				parser->state = HPSJAM_MIDI_ST_UNKNOWN;
				return (1);
			case HPSJAM_MIDI_ST_SYSEX_2:
				parser->temp_1[0] = p0 | 0x07;
				parser->temp_1[3] = 0xf7;
				parser->temp_cmd = parser->temp_1;
				parser->state = HPSJAM_MIDI_ST_UNKNOWN;
				return (1);
			}
			parser->state = HPSJAM_MIDI_ST_UNKNOWN;
			break;
		}
	} else if (b >= 0x80) {
		parser->temp_1[1] = b;
		if ((b >= 0xc0) && (b <= 0xdf)) {
			parser->state = HPSJAM_MIDI_ST_1PARAM;
		} else {
			parser->state = HPSJAM_MIDI_ST_2PARAM_1;
		}
	} else {			/* b < 0x80 */
		switch (parser->state) {
		case HPSJAM_MIDI_ST_1PARAM:
			if (parser->temp_1[1] < 0xf0) {
				p0 |= parser->temp_1[1] >> 4;
			} else {
				p0 |= 0x02;
				parser->state = HPSJAM_MIDI_ST_UNKNOWN;
			}
			parser->temp_1[0] = p0;
			parser->temp_1[2] = b;
			parser->temp_1[3] = 0;
			parser->temp_cmd = parser->temp_1;
			return (1);
		case HPSJAM_MIDI_ST_2PARAM_1:
			parser->temp_1[2] = b;
			parser->state = HPSJAM_MIDI_ST_2PARAM_2;
			break;
		case HPSJAM_MIDI_ST_2PARAM_2:
			if (parser->temp_1[1] < 0xf0) {
				p0 |= parser->temp_1[1] >> 4;
				parser->state = HPSJAM_MIDI_ST_2PARAM_1;
			} else {
				p0 |= 0x03;
				parser->state = HPSJAM_MIDI_ST_UNKNOWN;
			}
			parser->temp_1[0] = p0;
			parser->temp_1[3] = b;
			parser->temp_cmd = parser->temp_1;
			return (1);
		case HPSJAM_MIDI_ST_SYSEX_0:
			parser->temp_1[1] = b;
			parser->state = HPSJAM_MIDI_ST_SYSEX_1;
			break;
		case HPSJAM_MIDI_ST_SYSEX_1:
			parser->temp_1[2] = b;
			parser->state = HPSJAM_MIDI_ST_SYSEX_2;
			break;
		case HPSJAM_MIDI_ST_SYSEX_2:
			parser->temp_1[0] = p0 | 0x04;
			parser->temp_1[3] = b;
			parser->temp_cmd = parser->temp_1;
			parser->state = HPSJAM_MIDI_ST_SYSEX_0;
			return (1);
		default:
			break;
		}
	}
	return (0);
}

int
hpsjam_client_peer :: midi_process(uint8_t *buffer)
{
	QMutexLocker locker(&lock);
	uint8_t data[1];
	int retval = 0;

	/* Duplicate all NOTE OFF events, to kill of any hanging notes. */
	if (in_midi_escaped[0]) {
		in_midi_escaped[2] = 0;	/* clear the velocity */
		memcpy(buffer, in_midi_escaped, 3);
		memset(in_midi_escaped, 0, sizeof(in_midi_escaped));
		return (3);
	}

	/* Read from MIDI buffer. */
	while (in_midi.remData(data, sizeof(data))) {
		if (hpsjam_midi_convert_to_usb(&in_midi_parse, 0, data[0])) {
			retval = hpsjam_midi_cmd_to_len[in_midi_parse.temp_cmd[0] & 0xF];
			if (retval == 0)
				continue;
			assert(retval > 0);
			assert(retval < 5);
			memcpy(buffer, &in_midi_parse.temp_cmd[1], retval);
			/* Make a copy of NOTE OFF events. */
			if (retval == 3 && (buffer[0] & 0xF0) == 0x80)
				memcpy(in_midi_escaped, buffer, retval);
			break;
		}
	}
	return (retval);
}

void
hpsjam_client_peer :: sound_process(float *left, float *right, size_t samples)
{
	QMutexLocker locker(&lock);

	if (address[0].valid() == false) {
		const bool effects = audio_effects.isActive();

		hpsjam_client->pushRecord(left, right, samples);

		if (effects) {
			for (size_t x = 0; x != samples; x++) {
				float temp = audio_effects.getSample();

				left[x] = temp;
				right[x] = temp;
			}
		} else {
			memset(left, 0, sizeof(left[0]) * samples);
			memset(right, 0, sizeof(right[0]) * samples);
		}

		if (hpsjam_client->pullPlayback(left, right, samples) && effects) {
			/* May need compressor */
			for (size_t x = 0; x != samples; x++) {
				hpsjam_stereo_compressor(HPSJAM_SAMPLE_RATE,
				    local_peak, left[x], right[x]);
			}
		}
		return;
	}

	/* compute levels */
	out_level[0].addSamples(left, samples);
	out_level[1].addSamples(right, samples);

	float temp_l[samples];
	float temp_r[samples];

	/* Make a copy of input */
	memcpy(temp_l, left, sizeof(temp_l));
	memcpy(temp_r, right, sizeof(temp_r));

	/* Process bits */
	if (bits & HPSJAM_BIT_MUTE) {
		memset(left, 0, sizeof(left[0]) * samples);
		memset(right, 0, sizeof(right[0]) * samples);
	}

	hpsjam_client->pullPlayback(left, right, samples);

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

	out_audio[0].addSamples(left, samples);
	out_audio[1].addSamples(right, samples);

	in_audio[0].remSamples(left, samples);
	in_audio[1].remSamples(right, samples);

	/* Process bits */
	if (bits & HPSJAM_BIT_SOLO) {
		memset(left, 0, sizeof(left[0]) * samples);
		memset(right, 0, sizeof(right[0]) * samples);
	}

	/* Process local equalizer */
	local_eq.doit(temp_l, temp_r, samples);

	/* Balance fader */
	const float mg[2] = {
		(bits & HPSJAM_BIT_INVERT) ? - mon_gain[0] : mon_gain[0],
		mon_gain[1],
	};

	/* Add monitor */
	if (mg[0] != 0.0f) {
		/* Process panning and balance */
		if (mon_pan < 0.0f) {
			const float g[3] = { 1.0f + mon_pan, 2.0f + mon_pan, - mon_pan };
			for (size_t x = 0; x != samples; x++) {
				float l = (temp_l[x] * g[1] + temp_r[x] * g[2]) / 2.0f;
				float r = temp_r[x] * g[0];

				left[x] = left[x] * mg[1] + l * mg[0];
				right[x] = right[x] * mg[1] + r * mg[0];
			}
		} else if (mon_pan > 0.0f) {
			const float g[3] = { 1.0f - mon_pan, 2.0f - mon_pan, mon_pan };
			for (size_t x = 0; x != samples; x++) {
				float l = temp_l[x] * g[0];
				float r = (temp_r[x] * g[1] + temp_l[x] * g[2]) / 2.0f;

				left[x] = left[x] * mg[1] + l * mg[0];
				right[x] = right[x] * mg[1] + r * mg[0];
			}
		} else {
			for (size_t x = 0; x != samples; x++) {
				left[x] = left[x] * mg[1] + temp_l[x] * mg[0];
				right[x] = right[x] * mg[1] + temp_r[x] * mg[0];
			}
		}
	}

	/* Add audio effects, if any */
	if (audio_effects.isActive()) {
		for (size_t x = 0; x != samples; x++) {
			float temp = audio_effects.getSample();

			left[x] += temp;
			right[x] += temp;
		}
	}

#ifdef HAVE_HTTPD
	/* Stream the audio output, if any */
	if (http_nstate != 0)
		hpsjam_httpd_streamer(left, right, samples);
#endif
	/* Process final compressor */
	for (size_t x = 0; x != samples; x++) {
		hpsjam_stereo_compressor(HPSJAM_SAMPLE_RATE,
		    local_peak, left[x], right[x]);
	}

	hpsjam_client->pushRecord(left, right, samples);
}

size_t
hpsjam_server_peer :: serverID()
{
	return (this - hpsjam_server_peers);
}

void
hpsjam_server_peer :: handle_pending_watchdog()
{
	QMutexLocker locker(&lock);

	if (address[0].valid() && output_pkt.empty()) {
		struct hpsjam_packet_entry *pkt = new struct hpsjam_packet_entry;
		pkt->packet.setPing(0, hpsjam_ticks, 0, 0);
		pkt->packet.type = HPSJAM_TYPE_PING_REQUEST;
		pkt->insert_tail(&output_pkt.head);
	}
}

void
hpsjam_server_peer :: handle_pending_timeout()
{
	struct hpsjam_packet_entry *pkt;

	QMutexLocker locker(&lock);
	init();
	locker.unlock();

	/* tell other clients about disconnect */
	pkt = new struct hpsjam_packet_entry;
	pkt->packet.setFaderData(0, serverID(), 0, 0);
	pkt->packet.type = HPSJAM_TYPE_FADER_DISCONNECT_REPLY;
	hpsjam_server_broadcast(*pkt, this);
	delete pkt;
}

template <typename T>
void HpsJamProcessOutputAudio(T &s, float *left, float *right)
{
	/* check if we should downsample to mono */
	switch (s.output_fmt) {
	case HPSJAM_TYPE_AUDIO_8_BIT_1CH:
	case HPSJAM_TYPE_AUDIO_16_BIT_1CH:
	case HPSJAM_TYPE_AUDIO_24_BIT_1CH:
	case HPSJAM_TYPE_AUDIO_32_BIT_1CH:
		for (unsigned int x = 0; x != HPSJAM_DEF_SAMPLES; x++)
			left[x] = right[x] = (left[x] + right[x]) / 2.0f;
		break;
	default:
		break;
	}

	/* run compressor */
	for (unsigned int x = 0; x != HPSJAM_DEF_SAMPLES; x++) {
		hpsjam_stereo_compressor(HPSJAM_SAMPLE_RATE,
		    s.out_peak, left[x], right[x]);
	}

	/* add samples to final output buffer */
	s.out_buffer[0].addSamples(left, HPSJAM_DEF_SAMPLES);
	s.out_buffer[1].addSamples(right, HPSJAM_DEF_SAMPLES);
}

static uint8_t hpsjam_midi_data[16];
static size_t hpsjam_midi_bufsize;

template <typename T>
void HpsJamSendPacket(T &s)
{
	struct hpsjam_packet_entry entry;
	float temp[2][HPSJAM_NOM_SAMPLES];

	/* append MIDI data, if any */
	if (hpsjam_midi_bufsize != 0) {
		entry.packet.putMidiData(hpsjam_midi_data, hpsjam_midi_bufsize);
		s.output_pkt.append_pkt(entry);
	}

	/* check if we are sending XOR data */
	if (s.output_pkt.isXorFrame())
		goto done;

	/* get back correct amount of samples */
	s.out_buffer[0].remSamples(temp[0], HPSJAM_NOM_SAMPLES);
	s.out_buffer[1].remSamples(temp[1], HPSJAM_NOM_SAMPLES);

	/* select output format */
	switch (s.output_fmt) {
	case HPSJAM_TYPE_AUDIO_8_BIT_1CH:
		entry.packet.put8Bit1ChSample(temp[0], HPSJAM_NOM_SAMPLES);
		s.output_pkt.append_pkt(entry);
		break;
	case HPSJAM_TYPE_AUDIO_16_BIT_1CH:
		entry.packet.put16Bit1ChSample(temp[0], HPSJAM_NOM_SAMPLES);
		s.output_pkt.append_pkt(entry);
		break;
	case HPSJAM_TYPE_AUDIO_24_BIT_1CH:
		entry.packet.put24Bit1ChSample(temp[0], HPSJAM_NOM_SAMPLES);
		s.output_pkt.append_pkt(entry);
		break;
	case HPSJAM_TYPE_AUDIO_32_BIT_1CH:
		entry.packet.put32Bit1ChSample(temp[0], HPSJAM_NOM_SAMPLES);
		s.output_pkt.append_pkt(entry);
		break;
	case HPSJAM_TYPE_AUDIO_8_BIT_2CH:
		entry.packet.put8Bit2ChSample(temp[0], temp[1], HPSJAM_NOM_SAMPLES);
		s.output_pkt.append_pkt(entry);
		break;
	case HPSJAM_TYPE_AUDIO_16_BIT_2CH:
		entry.packet.put16Bit2ChSample(temp[0], temp[1], HPSJAM_NOM_SAMPLES);
		s.output_pkt.append_pkt(entry);
		break;
	case HPSJAM_TYPE_AUDIO_24_BIT_2CH:
		entry.packet.put24Bit2ChSample(temp[0], temp[1], HPSJAM_NOM_SAMPLES);
		s.output_pkt.append_pkt(entry);
		break;
	case HPSJAM_TYPE_AUDIO_32_BIT_2CH:
		entry.packet.put32Bit2ChSample(temp[0], temp[1], HPSJAM_NOM_SAMPLES);
		s.output_pkt.append_pkt(entry);
		break;
	default:
		entry.packet.putSilence(HPSJAM_NOM_SAMPLES);
		s.output_pkt.append_pkt(entry);
		break;
	}
done:
	/* send a packet */
	if (s.multi_port && (s.multi_wait == 0 || s.multi_wait-- == 0))
		s.output_pkt.send(s.address[s.output_pkt.seqno % HPSJAM_PORTS_MAX]);
	else
		s.output_pkt.send(s.address[0]);
}

template <typename T>
bool HpsJamReceiveUnSequenced(T &s, const struct hpsjam_packet *ptr, float *temp)
{
	size_t num;

	switch (ptr->type) {
	case HPSJAM_TYPE_AUDIO_8_BIT_1CH:
		num = ptr->get8Bit1ChSample(temp);
		assert(num <= HPSJAM_MAX_PKT);
		s.in_audio[0].addSamples(temp, num);
		s.in_audio[1].addSamples(temp, num);
		s.in_level[0].addSamples(temp, num);
		s.in_level[1].addSamples(temp, num);
		return (true);
	case HPSJAM_TYPE_AUDIO_16_BIT_1CH:
		num = ptr->get16Bit1ChSample(temp);
		assert(num <= HPSJAM_MAX_PKT);
		s.in_audio[0].addSamples(temp, num);
		s.in_audio[1].addSamples(temp, num);
		s.in_level[0].addSamples(temp, num);
		s.in_level[1].addSamples(temp, num);
		return (true);
	case HPSJAM_TYPE_AUDIO_24_BIT_1CH:
		num = ptr->get24Bit1ChSample(temp);
		assert(num <= HPSJAM_MAX_PKT);
		s.in_audio[0].addSamples(temp, num);
		s.in_audio[1].addSamples(temp, num);
		s.in_level[0].addSamples(temp, num);
		s.in_level[1].addSamples(temp, num);
		return (true);
	case HPSJAM_TYPE_AUDIO_32_BIT_1CH:
		num = ptr->get32Bit1ChSample(temp);
		assert(num <= HPSJAM_MAX_PKT);
		s.in_audio[0].addSamples(temp, num);
		s.in_audio[1].addSamples(temp, num);
		s.in_level[0].addSamples(temp, num);
		s.in_level[1].addSamples(temp, num);
		return (true);
	case HPSJAM_TYPE_AUDIO_8_BIT_2CH:
		num = ptr->get8Bit2ChSample(temp, temp + (HPSJAM_MAX_PKT / 2));
		assert(num <= (HPSJAM_MAX_PKT / 2));
		s.in_audio[0].addSamples(temp, num);
		s.in_audio[1].addSamples(temp + (HPSJAM_MAX_PKT / 2), num);
		s.in_level[0].addSamples(temp, num);
		s.in_level[1].addSamples(temp + (HPSJAM_MAX_PKT / 2), num);
		return (true);
	case HPSJAM_TYPE_AUDIO_16_BIT_2CH:
		num = ptr->get16Bit2ChSample(temp, temp + (HPSJAM_MAX_PKT / 2));
		assert(num <= (HPSJAM_MAX_PKT / 2));
		s.in_audio[0].addSamples(temp, num);
		s.in_audio[1].addSamples(temp + (HPSJAM_MAX_PKT / 2), num);
		s.in_level[0].addSamples(temp, num);
		s.in_level[1].addSamples(temp + (HPSJAM_MAX_PKT / 2), num);
		return (true);
	case HPSJAM_TYPE_AUDIO_24_BIT_2CH:
		num = ptr->get24Bit2ChSample(temp, temp + (HPSJAM_MAX_PKT / 2));
		assert(num <= (HPSJAM_MAX_PKT / 2));
		s.in_audio[0].addSamples(temp, num);
		s.in_audio[1].addSamples(temp + (HPSJAM_MAX_PKT / 2), num);
		s.in_level[0].addSamples(temp, num);
		s.in_level[1].addSamples(temp + (HPSJAM_MAX_PKT / 2), num);
		return (true);
	case HPSJAM_TYPE_AUDIO_32_BIT_2CH:
		num = ptr->get32Bit2ChSample(temp, temp + (HPSJAM_MAX_PKT / 2));
		assert(num <= (HPSJAM_MAX_PKT / 2));
		s.in_audio[0].addSamples(temp, num);
		s.in_audio[1].addSamples(temp + (HPSJAM_MAX_PKT / 2), num);
		s.in_level[0].addSamples(temp, num);
		s.in_level[1].addSamples(temp + (HPSJAM_MAX_PKT / 2), num);
		return (true);
	case HPSJAM_TYPE_AUDIO_32_BIT_2CH + 1 ... HPSJAM_TYPE_AUDIO_MAX:
		return (true);
	case HPSJAM_TYPE_MIDI_PACKET:
		num = HPSJAM_MAX_PKT * sizeof(temp[0]);
		if (ptr->getMidiData((uint8_t *)temp, &num))
			s.in_midi.addData((uint8_t *)temp, num);
		return (true);
	case HPSJAM_TYPE_AUDIO_SILENCE:
		num = ptr->getSilence();
		s.in_audio[0].addSilence(num);
		s.in_audio[1].addSilence(num);
		return (true);
	case HPSJAM_TYPE_ACK:
		/* check if other side received packet */
		if (ptr->getPeerSeqNo() == s.output_pkt.pend_seqno)
			s.output_pkt.advance();
		return (true);
	default:
		return (false);
	}
}

static unsigned hpsjam_server_adjust[3];

void
hpsjam_server_peer :: audio_export()
{
	const union hpsjam_frame *pkt;
	const struct hpsjam_packet *ptr;
	struct hpsjam_packet_entry *pres;
	float temp[HPSJAM_MAX_PKT];
	size_t num;

	QMutexLocker locker(&lock);

	if (valid == false) {
		memset(tmp_audio, 0, sizeof(tmp_audio));
		return;
	}

	while ((pkt = input_pkt.first_pkt())) {
		for (ptr = pkt->start; ptr->valid(pkt->end); ptr = ptr->next()) {
			/* check for unsequenced packets */
			if (HpsJamReceiveUnSequenced
			    <class hpsjam_server_peer>(*this, ptr, temp))
				continue;
			/* check if other side received packet */
			if (ptr->getPeerSeqNo() == output_pkt.pend_seqno)
				output_pkt.advance();
			/* check if sequence number matches */
			if (ptr->getLocalSeqNo() != output_pkt.peer_seqno)
				continue;
			/* advance expected sequence number */
			output_pkt.peer_seqno++;
			output_pkt.send_ack = true;

			switch (ptr->type) {
			uint16_t packets;
			uint16_t time_ms;
			uint32_t features;
			uint64_t passwd;
			uint8_t mix;
			uint8_t index;
			const char *data;
			size_t len;

			case HPSJAM_TYPE_CONFIGURE_REQUEST:
				if (ptr->getConfigure(output_fmt))
					break;
				output_fmt = HPSJAM_TYPE_AUDIO_SILENCE;
				break;
			case HPSJAM_TYPE_PING_REQUEST:
				if (ptr->getPing(packets, time_ms, passwd, features) &&
				    output_pkt.find(HPSJAM_TYPE_PING_REPLY) == 0) {
					if (hpsjam_no_multi_port)
						features &= ~HPSJAM_FEATURE_MULTI_PORT;

					pres = new struct hpsjam_packet_entry;
					pres->packet.setPing(0, time_ms, 0, features & HPSJAM_FEATURE_MULTI_PORT);
					pres->packet.type = HPSJAM_TYPE_PING_REPLY;
					pres->insert_tail(&output_pkt.head);

					if (features & HPSJAM_FEATURE_MULTI_PORT)
						multi_port = true;
				}
				break;
			case HPSJAM_TYPE_ICON_REQUEST:
				if (ptr->getRawData(&data, len)) {
					/* prepend username */
					icon = QByteArray(data, len);

					pres = new struct hpsjam_packet_entry;
					pres->packet.setFaderData(0, serverID(), icon.constData(), icon.length());
					pres->packet.type = HPSJAM_TYPE_FADER_ICON_REPLY;
					pres->insert_tail(&output_pkt.head);
					hpsjam_server_broadcast(*pres, this);

					/* tell this client about other icons */
					for (unsigned x = 0; x != hpsjam_num_server_peers; x++) {
						if (hpsjam_server_peers + x == this)
							continue;
						class hpsjam_server_peer &peer = hpsjam_server_peers[x];
						QMutexLocker locker(&peer.lock);
						if (peer.valid == false)
							continue;
						QByteArray &t = peer.icon;
						pres = new struct hpsjam_packet_entry;
						pres->packet.setFaderData(0, x, t.constData(), t.length());
						pres->packet.type = HPSJAM_TYPE_FADER_ICON_REPLY;
						pres->insert_tail(&output_pkt.head);
					}
				}
				break;
			case HPSJAM_TYPE_NAME_REQUEST:
				if (ptr->getRawData(&data, len)) {
					/* prepend username */
					QByteArray t(data, len);
					name = QString::fromUtf8(t);

					pres = new struct hpsjam_packet_entry;
					pres->packet.setFaderData(0, serverID(), t.constData(), t.length());
					pres->packet.type = HPSJAM_TYPE_FADER_NAME_REPLY;
					pres->insert_tail(&output_pkt.head);
					hpsjam_server_broadcast(*pres, this);

					/* tell this client about other names */
					for (unsigned x = 0; x != hpsjam_num_server_peers; x++) {
						if (hpsjam_server_peers + x == this)
							continue;
						class hpsjam_server_peer &peer = hpsjam_server_peers[x];
						QMutexLocker locker(&peer.lock);
						if (peer.valid == false)
							continue;
						t = peer.name.toUtf8();
						pres = new struct hpsjam_packet_entry;
						pres->packet.setFaderData(0, x, t.constData(), t.length());
						pres->packet.type = HPSJAM_TYPE_FADER_NAME_REPLY;
						pres->insert_tail(&output_pkt.head);
					}
				}
				break;
			case HPSJAM_TYPE_LYRICS_REQUEST:
				pres = new struct hpsjam_packet_entry;
				if (ptr->getRawData(&data, len)) {
					QByteArray t(data, len);

					/* echo back lyrics */
					pres = new struct hpsjam_packet_entry;
					pres->packet.setRawData(t.constData(), t.length());
					pres->packet.type = HPSJAM_TYPE_LYRICS_REPLY;
					pres->insert_tail(&output_pkt.head);
					hpsjam_server_broadcast(*pres, this);
				}
				break;
			case HPSJAM_TYPE_CHAT_REQUEST:
				if (ptr->getRawData(&data, len)) {
					/* prepend username */
					QByteArray t(data, len);
					QString str = QString::fromUtf8(t);
					str.prepend(QString("[") + name + QString("]: "));
					str.truncate(128 + 32 + 4);
					t = str.toUtf8();

					/* echo back text */
					pres = new struct hpsjam_packet_entry;
					pres->packet.setRawData(t.constData(), t.length());
					pres->packet.type = HPSJAM_TYPE_CHAT_REPLY;
					pres->insert_tail(&output_pkt.head);
					hpsjam_server_broadcast(*pres, this);
				}
				break;
			case HPSJAM_TYPE_FADER_GAIN_REQUEST:
				if (allow_mixer_access == false)
					break;
				if (ptr->getFaderValue(mix, index, temp, num)) {
					assert(num <= HPSJAM_MAX_PKT);
					if (mix != 0 || num <= 0)
						break;
					if (index + num > hpsjam_num_server_peers)
						break;

					/* echo gain */
					pres = new struct hpsjam_packet_entry;
					pres->packet.setFaderValue(mix, index, temp, num);
					pres->packet.type = HPSJAM_TYPE_FADER_GAIN_REPLY;
					hpsjam_server_broadcast(*pres, this);
					delete pres;

					/* local gain */
					for (size_t x = 0; x != num; x++) {
						pres = new struct hpsjam_packet_entry;
						pres->packet.setFaderValue(0, 0, temp + x, 1);
						pres->packet.type = HPSJAM_TYPE_LOCAL_GAIN_REPLY;

						if (index + x == serverID()) {
							pres->insert_tail(&output_pkt.head);
							gain = temp[x];
						} else {
							hpsjam_server_peer &peer = hpsjam_server_peers[index + x];
							QMutexLocker peer_locker(&peer.lock);
							pres->insert_tail(&peer.output_pkt.head);
							peer.gain = temp[x];
						}
					}
				}
				break;
			case HPSJAM_TYPE_FADER_PAN_REQUEST:
				if (allow_mixer_access == false)
					break;
				if (ptr->getFaderValue(mix, index, temp, num)) {
					assert(num <= HPSJAM_MAX_PKT);
					if (mix != 0 || num <= 0)
						break;
					if (index + num > hpsjam_num_server_peers)
						break;

					/* echo pan */
					pres = new struct hpsjam_packet_entry;
					pres->packet.setFaderValue(mix, index, temp, num);
					pres->packet.type = HPSJAM_TYPE_FADER_PAN_REPLY;
					hpsjam_server_broadcast(*pres, this);
					delete pres;

					/* local pan */
					for (size_t x = 0; x != num; x++) {
						pres = new struct hpsjam_packet_entry;
						pres->packet.setFaderValue(0, 0, temp + x, 1);
						pres->packet.type = HPSJAM_TYPE_LOCAL_PAN_REPLY;

						if (index + x == serverID()) {
							pres->insert_tail(&output_pkt.head);
							pan = temp[x];
						} else {
							hpsjam_server_peer &peer = hpsjam_server_peers[index + x];
							QMutexLocker peer_locker(&peer.lock);
							pres->insert_tail(&peer.output_pkt.head);
							peer.pan = temp[x];
						}
					}
				}
				break;
			case HPSJAM_TYPE_FADER_EQ_REQUEST:
				if (allow_mixer_access == false)
					break;
				if (ptr->getFaderData(mix, index, &data, num)) {
					if (mix != 0)
						break;
					if (index >= hpsjam_num_server_peers)
						break;

					/* echo EQ */
					pres = new struct hpsjam_packet_entry;
					pres->packet.setFaderData(mix, index, data, num);
					pres->packet.type = HPSJAM_TYPE_FADER_EQ_REPLY;
					hpsjam_server_broadcast(*pres, this);

					pres->packet.setFaderData(0, 0, data, num);
					pres->packet.type = HPSJAM_TYPE_LOCAL_EQ_REPLY;

					/* local EQ */
					if (index == serverID()) {
						pres->insert_tail(&output_pkt.head);
						delete [] eq_data;
						eq_data = new char [eq_size = num];
						memcpy(eq_data, data, eq_size);
					} else {
						hpsjam_server_peer &peer = hpsjam_server_peers[index];
						QMutexLocker other(&peer.lock);
						pres->insert_tail(&peer.output_pkt.head);
						delete [] peer.eq_data;
						peer.eq_data = new char [peer.eq_size = num];
						memcpy(peer.eq_data, data, peer.eq_size);
					}
				}
				break;

			case HPSJAM_TYPE_FADER_BITS_REQUEST:
				if (ptr->getFaderData(mix, index, &data, num)) {
					if (mix != 0 || num <= 0)
						break;
					if (index + num > hpsjam_num_server_peers)
						break;
					/* copy bits in place */
					memcpy(bits + index, data, num);
				}
				break;
			default:
				break;
			}
		}
	}

	/* send a ping, if idle */
	if (output_pkt.empty()) {
		pres = new struct hpsjam_packet_entry;
		pres->packet.setPing(0, hpsjam_ticks, 0, 0);
		pres->packet.type = HPSJAM_TYPE_PING_REQUEST;
		pres->insert_tail(&output_pkt.head);
	}

	/* extract samples for this tick */
	in_audio[0].remSamples(tmp_audio[0], HPSJAM_DEF_SAMPLES);
	in_audio[1].remSamples(tmp_audio[1], HPSJAM_DEF_SAMPLES);

	/* check if we should adjust the timer */
	hpsjam_server_adjust[in_audio[0].getLowWater()]++;
}

void
hpsjam_server_peer :: audio_import()
{
	QMutexLocker locker(&lock);

	if (valid == false)
		return;

	/* process output audio */
	HpsJamProcessOutputAudio
	    <class hpsjam_server_peer>(*this, out_audio[0], out_audio[1]);

	/* send a packet */
	HpsJamSendPacket
	    <class hpsjam_server_peer>(*this);
}

void
hpsjam_server_peer :: send_welcome_message()
{
	if (hpsjam_welcome_message_file == 0)
		return;

	QFile file(hpsjam_welcome_message_file);

	if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
		return;

	QTextStream in(&file);

	while (in.atEnd() == false) {
		QString line = in.readLine();
		line.truncate(128);

		QByteArray t = line.toUtf8();

		struct hpsjam_packet_entry *pres = new struct hpsjam_packet_entry;
		pres->packet.setRawData(t.constData(), t.length());
		pres->packet.type = HPSJAM_TYPE_CHAT_REPLY;
		pres->insert_tail(&output_pkt.head);
	}
}

static void
hpsjam_send_levels()
{
	constexpr size_t maxLevel = 32;
	static unsigned group;
	struct hpsjam_packet_entry entry;
	float level_temp[maxLevel][2];

	if (hpsjam_ticks % 128)
		return;

	for (unsigned x = 0; x != maxLevel; x++) {
		unsigned index = x + group * maxLevel;

		if (index >= hpsjam_num_server_peers) {
			level_temp[x][0] = 0.0f;
			level_temp[x][1] = 0.0f;
			continue;
		}

		QMutexLocker locker(&hpsjam_server_peers[index].lock);
		if (hpsjam_server_peers[index].valid) {
			level_temp[x][0] = hpsjam_server_peers[index].in_level[0].getLevel();
			level_temp[x][1] = hpsjam_server_peers[index].in_level[1].getLevel();
		} else {
			level_temp[x][0] = 0.0f;
			level_temp[x][1] = 0.0f;
		}
	}
	entry.packet.setFaderValue(0, group * maxLevel, level_temp[0], 2 * maxLevel);
	entry.packet.type = HPSJAM_TYPE_FADER_LEVEL_REPLY;
	hpsjam_server_broadcast(entry, 0, true);

	/* advance to next group */
	group++;
	if ((group * maxLevel) >= hpsjam_num_server_peers)
		group = 0;
}

void
hpsjam_server_peer :: send_mixer_parameters()
{
	constexpr size_t maxLevel = 32;
	unsigned group_max = (hpsjam_num_server_peers + maxLevel - 1) / maxLevel;
	hpsjam_packet_entry *pres;
	float gain_temp[maxLevel];
	float pan_temp[maxLevel];

	for (unsigned group = 0; group != group_max; group++) {
		for (unsigned x = 0; x != maxLevel; x++) {
			const unsigned index = x + group * maxLevel;

			if (index >= hpsjam_num_server_peers) {
				gain_temp[x] = 1.0f;
				pan_temp[x] = 0.0f;
				continue;
			}

			if (index == serverID()) {
				gain_temp[x] = gain;
				pan_temp[x] = pan;
			} else {
				hpsjam_server_peer &peer = hpsjam_server_peers[index];
				QMutexLocker locker(&peer.lock);

				if (peer.valid) {
					gain_temp[x] = peer.gain;
					pan_temp[x] = peer.pan;
				} else {
					gain_temp[x] = 1.0f;
					pan_temp[x] = 0.0f;
				}
			}
		}

		pres = new struct hpsjam_packet_entry;
		pres->packet.setFaderValue(0, group * maxLevel, gain_temp, maxLevel);
		pres->packet.type = HPSJAM_TYPE_FADER_GAIN_REPLY;
		pres->insert_tail(&output_pkt.head);

		pres = new struct hpsjam_packet_entry;
		pres->packet.setFaderValue(0, group * maxLevel, pan_temp, maxLevel);
		pres->packet.type = HPSJAM_TYPE_FADER_PAN_REPLY;
		pres->insert_tail(&output_pkt.head);
	}

	for (unsigned index = 0; index < hpsjam_num_server_peers; index++) {
		if (index == serverID())
			continue;

		hpsjam_server_peer &peer = hpsjam_server_peers[index];
		QMutexLocker locker(&peer.lock);

		if (peer.valid == false || peer.eq_size == 0)
			continue;
		pres = new struct hpsjam_packet_entry;
		pres->packet.setFaderData(0, index, peer.eq_data, peer.eq_size);
		pres->packet.type = HPSJAM_TYPE_FADER_EQ_REPLY;
		pres->insert_tail(&output_pkt.head);
	}
}

static inline float
float_gain(float value, int32_t gain)
{
	return (value * gain) * (1.0f / 256.0f);
}

static uint32_t
get_gain_from_bits(uint8_t value)
{
	int32_t temp = HPSJAM_BIT_GAIN_GET(value);

	/* sign extend to fit float exponent */
	temp <<= (32 - 5);
	temp >>= (32 - 5);

	return (powf(256.0f, (temp + 16) / 16.0f));
}

static struct hpsjam_server_default_mix hpsjam_server_default_mix[HPSJAM_CPU_MAX];
hpsjam_midi_buffer *hpsjam_default_midi;

void
hpsjam_server_peer :: audio_mixing()
{
	QMutexLocker locker(&lock);

	if (valid == false) {
		/* clear output audio */
		memset(out_audio, 0, sizeof(out_audio));
		return;
	}

	for (unsigned y = 0; y != hpsjam_num_server_peers; y++) {
		if (bits[y] & HPSJAM_BIT_SOLO)
			goto do_solo;
	}

	/* use the default mix as a starting point */
	assert(sizeof(out_audio) == sizeof(hpsjam_server_default_mix[0].out_audio));
	memcpy(out_audio, hpsjam_server_default_mix[0].out_audio, sizeof(out_audio));

	for (unsigned y = 0; y != hpsjam_num_server_peers; y++) {
		const class hpsjam_server_peer &other = hpsjam_server_peers[y];

		if (other.valid == false || bits[y] == 0)
			continue;
		if (bits[y] & HPSJAM_BIT_MUTE) {
			for (unsigned z = 0; z != HPSJAM_DEF_SAMPLES; z++) {
				/* silence own mix */
				out_audio[0][z] -= other.tmp_audio[0][z];
				out_audio[1][z] -= other.tmp_audio[1][z];
			}
		} else if (bits[y] & HPSJAM_BIT_INVERT) {
			const int32_t gain = get_gain_from_bits(bits[y]) + 256;

			for (unsigned z = 0; z != HPSJAM_DEF_SAMPLES; z++) {
				/* adjust mix */
				out_audio[0][z] -= float_gain(other.tmp_audio[0][z], gain);
				out_audio[1][z] -= float_gain(other.tmp_audio[1][z], gain);
			}
		} else {
			const int32_t gain = get_gain_from_bits(bits[y]) - 256;

			for (unsigned z = 0; z != HPSJAM_DEF_SAMPLES; z++) {
				/* adjust mix */
				out_audio[0][z] += float_gain(other.tmp_audio[0][z], gain);
				out_audio[1][z] += float_gain(other.tmp_audio[1][z], gain);
			}
		}
	}
	return;

do_solo:
	/* clear output audio */
	memset(out_audio, 0, sizeof(out_audio));

	for (unsigned y = 0; y != hpsjam_num_server_peers; y++) {
		const class hpsjam_server_peer &other = hpsjam_server_peers[y];

		if (other.valid == false)
			continue;
		if (~bits[y] & HPSJAM_BIT_SOLO)
			continue;
		if (bits[y] & HPSJAM_BIT_INVERT) {
			const int32_t gain = get_gain_from_bits(bits[y]);

			for (unsigned z = 0; z != HPSJAM_DEF_SAMPLES; z++) {
				out_audio[0][z] -= float_gain(other.tmp_audio[0][z], gain);
				out_audio[1][z] -= float_gain(other.tmp_audio[1][z], gain);
			}
		} else {
			const int32_t gain = get_gain_from_bits(bits[y]);

			for (unsigned z = 0; z != HPSJAM_DEF_SAMPLES; z++) {
				out_audio[0][z] += float_gain(other.tmp_audio[0][z], gain);
				out_audio[1][z] += float_gain(other.tmp_audio[1][z], gain);
			}
		}
	}
}

static void
hpsjam_server_get_audio(unsigned rem)
{
	uint8_t temp[hpsjam_midi_buffer::MIDI_BUFFER_MAX];
	size_t num;

	for (unsigned x = rem; x < hpsjam_num_server_peers; x += hpsjam_num_cpu) {
		class hpsjam_server_peer &peer = hpsjam_server_peers[x];

		/* export audio from data buffer, if any */
		peer.audio_export();

		/* create the default audio mix */
		for (unsigned z = 0; z != HPSJAM_DEF_SAMPLES; z++) {
			hpsjam_server_default_mix[rem].out_audio[0][z] +=
			    peer.tmp_audio[0][z];
			hpsjam_server_default_mix[rem].out_audio[1][z] +=
			    peer.tmp_audio[1][z];
		}

		/* create the default MIDI mix */
		num = peer.in_midi.remData(temp, sizeof(temp));
		if (num != 0)
			hpsjam_default_midi[rem].addData(temp, num);
	}
}

static void
hpsjam_server_audio_mixing(unsigned rem)
{
	for (unsigned x = rem; x < hpsjam_num_server_peers; x += hpsjam_num_cpu)
		hpsjam_server_peers[x].audio_mixing();
}

static void
hpsjam_server_audio_import(unsigned rem)
{
	for (unsigned x = rem; x < hpsjam_num_server_peers; x += hpsjam_num_cpu)
		hpsjam_server_peers[x].audio_import();
}

Q_DECL_EXPORT bool
hpsjam_server_tick()
{
	bool retval = false;

	/* reset timer adjustment */
	memset(hpsjam_server_adjust, 0, sizeof(hpsjam_server_adjust));

	/* reset the default server mix */
	memset(hpsjam_server_default_mix, 0, sizeof(hpsjam_server_default_mix[0]) * hpsjam_num_cpu);

	/* get audio */
	hpsjam_execute(&hpsjam_server_get_audio);

	/* merge audio and MIDI from each worker thread, if any */
	for (unsigned rem = 1; rem != hpsjam_num_cpu; rem++) {
		uint8_t temp[hpsjam_midi_buffer::MIDI_BUFFER_MAX];
		size_t num;

		for (unsigned z = 0; z != HPSJAM_DEF_SAMPLES; z++) {
			hpsjam_server_default_mix[0].out_audio[0][z] +=
			    hpsjam_server_default_mix[rem].out_audio[0][z];
			hpsjam_server_default_mix[0].out_audio[1][z] +=
			    hpsjam_server_default_mix[rem].out_audio[1][z];
		}

		/* create the default MIDI mix */
		num = hpsjam_default_midi[rem].remData(temp, sizeof(temp));
		if (num != 0)
			hpsjam_default_midi[0].addData(temp, num);
	}

#ifdef HAVE_HTTPD
	/* stream the default mix, if any */
	if (http_nstate != 0) {
		hpsjam_httpd_streamer(
		    hpsjam_server_default_mix[0].out_audio[0],
		    hpsjam_server_default_mix[0].out_audio[1],
		    HPSJAM_DEF_SAMPLES);
	}
#endif
	/* send out levels, if any */
	hpsjam_send_levels();

	/* mix everything */
	hpsjam_execute(&hpsjam_server_audio_mixing);

	/* prepare MIDI buffer, if any */
	if (hpsjam_midi_bufsize == 0) {
		hpsjam_midi_bufsize =
		  hpsjam_default_midi[0].remData(
		  hpsjam_midi_data, sizeof(hpsjam_midi_data));
	} else {
		hpsjam_midi_bufsize = 0 ;
	}

	/* send audio */
	hpsjam_execute(&hpsjam_server_audio_import);

	/* adjust timer, if any */
	if (hpsjam_server_adjust[1] >= hpsjam_server_adjust[0] &&
	    hpsjam_server_adjust[1] >= hpsjam_server_adjust[2]) {
		hpsjam_timer_adjust = 0;	/* go normal */
	} else if (hpsjam_server_adjust[0] >= hpsjam_server_adjust[1] &&
		   hpsjam_server_adjust[0] >= hpsjam_server_adjust[2]) {
		hpsjam_timer_adjust = 1;	/* go slower */
	} else {
		hpsjam_timer_adjust = -1;	/* go faster */
	}

	/* Adjust all buffers every 16 seconds approximately. */
	unsigned y = (hpsjam_ticks & 0x3fff);
	if (y < hpsjam_num_server_peers) {
		hpsjam_server_peer &peer = hpsjam_server_peers[y];

		QMutexLocker locker(&peer.lock);
		if (peer.valid) {
			peer.out_buffer[0].adjustBuffer();
			peer.out_buffer[1].adjustBuffer();
			peer.in_audio[0].adjustBuffer();
			peer.in_audio[1].adjustBuffer();
		}
	}

	for (unsigned x = 0; x != hpsjam_num_server_peers; x++) {
		hpsjam_server_peer &peer = hpsjam_server_peers[x];

		QMutexLocker locker(&peer.lock);
		if (peer.valid) {
			retval = true;
			break;
		}
	}
	return (retval);
}

void
hpsjam_client_peer :: handle_pending_watchdog()
{
	QMutexLocker locker(&lock);

	if (address[0].valid() && output_pkt.empty()) {
		struct hpsjam_packet_entry *pkt = new struct hpsjam_packet_entry;
		pkt->packet.setPing(0, hpsjam_ticks, 0, 0);
		pkt->packet.type = HPSJAM_TYPE_PING_REQUEST;
		pkt->insert_tail(&output_pkt.head);
	}
}

void
hpsjam_client_peer :: tick()
{
	QMutexLocker locker(&lock);

	if (address[0].valid() == false)
		return;

	const union hpsjam_frame *pkt;
	const struct hpsjam_packet *ptr;
	struct hpsjam_packet_entry *pres;
	union {
		float temp[HPSJAM_MAX_PKT];
		float audio[2][HPSJAM_DEF_SAMPLES];
	};
	size_t num;

	while ((pkt = input_pkt.first_pkt())) {
		for (ptr = pkt->start; ptr->valid(pkt->end); ptr = ptr->next()) {
			/* check for unsequenced packets */
			if (HpsJamReceiveUnSequenced
			    <class hpsjam_client_peer>(*this, ptr, temp))
				continue;
			/* check if other side received packet */
			if (ptr->getPeerSeqNo() == output_pkt.pend_seqno)
				output_pkt.advance();
			/* check if sequence number matches */
			if (ptr->getLocalSeqNo() != output_pkt.peer_seqno)
				continue;
			/* advance expected sequence number */
			output_pkt.peer_seqno++;
			output_pkt.send_ack = true;

			switch (ptr->type) {
			uint16_t packets;
			uint16_t time_ms;
			uint32_t features;
			uint64_t passwd;
			const char *data;
			uint8_t mix;
			uint8_t index;

			case HPSJAM_TYPE_PING_REQUEST:
				if (ptr->getPing(packets, time_ms, passwd, features) &&
				    output_pkt.find(HPSJAM_TYPE_PING_REPLY) == 0) {
					pres = new struct hpsjam_packet_entry;
					pres->packet.setPing(0, time_ms, 0, features & HPSJAM_FEATURE_MULTI_PORT);
					pres->packet.type = HPSJAM_TYPE_PING_REPLY;
					pres->insert_tail(&output_pkt.head);
				}
				break;
			case HPSJAM_TYPE_PING_REPLY:
				if (ptr->getPing(packets, time_ms, passwd, features)) {
					if (features & HPSJAM_FEATURE_MULTI_PORT)
						multi_port = true;
				}
				break;
			case HPSJAM_TYPE_LYRICS_REPLY:
				if (ptr->getRawData(&data, num)) {
					QByteArray t(data, num);
					emit receivedLyrics(new QString(QString::fromUtf8(t)));
				}
				break;
			case HPSJAM_TYPE_CHAT_REPLY:
				if (ptr->getRawData(&data, num)) {
					QByteArray t(data, num);
					emit receivedChat(new QString(QString::fromUtf8(t)));
				}
				break;
			case HPSJAM_TYPE_FADER_ICON_REPLY:
				if (ptr->getFaderData(mix, index, &data, num)) {
					if (mix != 0)
						break;
					if (self_index == -1) {
						self_index = index;
						emit receivedFaderSelf(mix, index);
					}
					emit receivedFaderIcon(mix, index, new QByteArray(data, num));
				}
				break;
			case HPSJAM_TYPE_FADER_NAME_REPLY:
				if (ptr->getFaderData(mix, index, &data, num)) {
					if (mix != 0)
						break;
					if (self_index == -1) {
						self_index = index;
						emit receivedFaderSelf(mix, index);
					}
					QByteArray t(data, num);
					emit receivedFaderName(mix, index, new QString(QString::fromUtf8(t)));
				}
				break;
			case HPSJAM_TYPE_FADER_GAIN_REPLY:
				if (ptr->getFaderValue(mix, index, temp, num)) {
					assert(num <= HPSJAM_MAX_PKT);
					if (mix != 0 || num <= 0)
						break;
					if (index + num > HPSJAM_PEERS_MAX)
						break;
					for (size_t x = 0; x != num; x++)
						emit receivedFaderGain(mix, index + x, temp[x]);
				}
				break;
			case HPSJAM_TYPE_FADER_PAN_REPLY:
				if (ptr->getFaderValue(mix, index, temp, num)) {
					assert(num <= HPSJAM_MAX_PKT);
					if (mix != 0 || num <= 0)
						break;
					if (index + num > HPSJAM_PEERS_MAX)
						break;
					for (size_t x = 0; x != num; x++)
						emit receivedFaderPan(mix, index + x, temp[x]);
				}
				break;
			case HPSJAM_TYPE_FADER_LEVEL_REPLY:
				if (ptr->getFaderValue(mix, index, temp, num)) {
					assert(num <= HPSJAM_MAX_PKT);
					if (mix != 0 || (num % 2) != 0 || num <= 0)
						break;
					if (index + (num / 2) > HPSJAM_PEERS_MAX)
						break;
					for (size_t x = 0; x != (num / 2); x++)
						emit receivedFaderLevel(mix, index + x, temp[2 * x], temp[2 * x + 1]);
				}
				break;
			case HPSJAM_TYPE_LOCAL_GAIN_REPLY:
				if (ptr->getFaderValue(mix, index, temp, num)) {
					assert(num <= HPSJAM_MAX_PKT);
					if (mix != 0 || index != 0 || num != 1)
						break;
					in_gain = temp[0];
				}
				break;
			case HPSJAM_TYPE_LOCAL_PAN_REPLY:
				if (ptr->getFaderValue(mix, index, temp, num)) {
					assert(num <= HPSJAM_MAX_PKT);
					if (mix != 0 || index != 0 || num != 1)
						break;
					in_pan = temp[0];
				}
				break;
			case HPSJAM_TYPE_FADER_EQ_REPLY:
				if (ptr->getFaderData(mix, index, &data, num)) {
					if (mix != 0)
						break;
					QByteArray t(data, num);
					emit receivedFaderEQ(mix, index, new QString(QString::fromLatin1(t)));
				}
				break;
			case HPSJAM_TYPE_LOCAL_EQ_REPLY:
				if (ptr->getFaderData(mix, index, &data, num)) {
					if (mix != 0 || index != 0)
						break;
					char *ptr = new char [num + 1];
					memcpy(ptr, data, num);
					ptr[num] = 0;
					eq.init(ptr);
					delete [] ptr;
				}
				break;
			case HPSJAM_TYPE_FADER_DISCONNECT_REPLY:
				if (ptr->getFaderData(mix, index, &data, num)) {
					if (mix != 0)
						break;
					emit receivedFaderDisconnect(mix, index);
				}
				break;
			default:
				break;
			}
		}
	}

	/* send a ping, if idle */
	if (output_pkt.empty()) {
		pres = new struct hpsjam_packet_entry;
		pres->packet.setPing(0, hpsjam_ticks, 0, 0);
		pres->packet.type = HPSJAM_TYPE_PING_REQUEST;
		pres->insert_tail(&output_pkt.head);
	}

	/* prepare MIDI buffer, if any */
	if (hpsjam_midi_bufsize == 0) {
		hpsjam_midi_bufsize =
		  hpsjam_default_midi[0].remData(
		  hpsjam_midi_data, sizeof(hpsjam_midi_data));
	} else {
		hpsjam_midi_bufsize = 0;
	}

	/* extract samples for this tick */
	out_audio[0].remSamples(audio[0], HPSJAM_DEF_SAMPLES);
	out_audio[1].remSamples(audio[1], HPSJAM_DEF_SAMPLES);

	/* check if we should adjust the timer */
	switch (out_audio[0].getLowWater()) {
	case 0:
		hpsjam_timer_adjust = 1;	/* go slower */
		break;
	case 1:
		hpsjam_timer_adjust = 0;	/* go normal */
		break;
	default:
		hpsjam_timer_adjust = -1;	/* go faster */
		break;
	}

	/* process output output */
	HpsJamProcessOutputAudio
	    <class hpsjam_client_peer>(*this, audio[0], audio[1]);

	/* send a packet */
	HpsJamSendPacket
	    <class hpsjam_client_peer>(*this);

	/* Adjust all buffers every 16 seconds approximately. */
	if ((hpsjam_ticks & 0x3fff) == 0) {
		out_audio[0].adjustBuffer();
		out_audio[1].adjustBuffer();
		in_audio[0].adjustBuffer();
		in_audio[1].adjustBuffer();
	}
}

void
hpsjam_client_peer :: handleChat(QString *str)
{
	hpsjam_client->w_chat->append(*str);
	delete str;
}

void
hpsjam_client_peer :: handleLyrics(QString *str)
{
	hpsjam_client->w_lyrics->append(*str);
	delete str;
}

void
hpsjam_cli_process(const struct hpsjam_socket_address &addr, const char *data, size_t len)
{
	struct hpsjam_packet_entry *pkt;
	QByteArray ba(data, len);
	QString str = QString::fromUtf8(ba);

	if (str.startsWith("set lyrics.text=")) {
		str.truncate(128 + 16);

		QByteArray temp = str.toUtf8();
		assert(temp.length() >= 16);

		if (hpsjam_num_server_peers == 0) {
			QMutexLocker locker(&hpsjam_client_peer->lock);

			if (hpsjam_client_peer->address[0].valid()) {
				/* send text */
				pkt = new struct hpsjam_packet_entry;
				pkt->packet.setRawData(temp.constData() + 16, temp.length() - 16);
				pkt->packet.type = HPSJAM_TYPE_LYRICS_REQUEST;
				pkt->insert_tail(&hpsjam_client_peer->output_pkt.head);
			}
		} else {
			pkt = new struct hpsjam_packet_entry;
			pkt->packet.setRawData(temp.constData() + 16, temp.length() - 16);
			pkt->packet.type = HPSJAM_TYPE_LYRICS_REPLY;
			hpsjam_server_broadcast(*pkt);
			delete pkt;
		}
	} else if (str.startsWith("kick=")) {
		int id = str.mid(5).toInt();

		if (hpsjam_num_server_peers == 0) {
			/* nothing to do */
		} else if (id > 0 && id <= (int)hpsjam_num_server_peers) {
			emit hpsjam_server_peers[id - 1].output_pkt.pendingTimeout();
		}
	}
}

static void
hpsjam_load_float_le32(const char *fname, int &off, int &max, float * &data)
{
	QFile file(fname);

	if (!file.open(QIODevice::ReadOnly)) {
		off = 0;
		max = 0;
		data = 0;
		return;
	}

	QByteArray *pba = new QByteArray(file.readAll());
	data = (float *)pba->data();
	off = max = pba->length() / 4;

	/* byte swap, if any */
	for (int x = 0; x != max; x++) {
		union {
			float data_float;
			uint32_t data_32;
			uint8_t data_8[4];
		} tmp;

		assert(sizeof(tmp) == 4);

		tmp.data_float = data[x];
		tmp.data_32 = tmp.data_8[0] | (tmp.data_8[1] << 8) |
		  (tmp.data_8[2] << 16) | (tmp.data_8[3] << 24);
		data[x] = tmp.data_float;
	}
}

hpsjam_client_audio_effects :: hpsjam_client_audio_effects()
{
	hpsjam_load_float_le32(":/sounds/new_message_float32le_48kHz_1ch.raw",
	    new_message_off, new_message_max, new_message_data);
	hpsjam_load_float_le32(":/sounds/new_user_float32le_48kHz_1ch.raw",
	    new_user_off, new_user_max, new_user_data);

	new_message_gain = 0.0f;
	new_user_gain = 0.0f;
}

void
hpsjam_client_audio_effects :: playNewMessage(float gain)
{
	if (new_message_off == new_message_max && gain > 0.0f) {
		new_message_off = 0;
		new_message_gain = gain;
	}
}

void
hpsjam_client_audio_effects :: playNewUser(float gain)
{
	if (new_user_off == new_user_max && gain > 0.0f) {
		new_user_off = 0;
		new_user_gain = gain;
	}
}

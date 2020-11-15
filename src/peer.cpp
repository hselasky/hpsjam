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

#include <QFile>
#include <QTextStream>
#include <QMutexLocker>

#include "hpsjam.h"
#include "peer.h"
#include "compressor.h"
#include "clientdlg.h"
#include "chatdlg.h"
#include "lyricsdlg.h"

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
			hpsjam_server_peers[x].send_welcome_message();
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

		QMutexLocker locker(&hpsjam_server_peers[x].lock);

		if (hpsjam_server_peers[x].valid == false)
			continue;

		/* check if a level packet is already pending */
		if (single && hpsjam_server_peers[x].output_pkt.find(entry.packet.type))
			continue;
		/* duplicate packet */
		ptr = new struct hpsjam_packet_entry;
		*ptr = entry;
		ptr->insert_tail(&hpsjam_server_peers[x].output_pkt.head);
	}
}

void
hpsjam_client_peer :: sound_process(float *left, float *right, size_t samples)
{
	QMutexLocker locker(&lock);

	if (address.valid() == false) {
		memset(left, 0, sizeof(left[0]) * samples);
		memset(right, 0, sizeof(right[0]) * samples);
		return;
	}

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

	out_level[0].addSamples(left, samples);
	out_level[1].addSamples(right, samples);

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

	/* Process final compressor */
	for (size_t x = 0; x != samples; x++) {
		hpsjam_stereo_compressor(HPSJAM_SAMPLE_RATE,
		    local_peak, left[x], right[x]);
	}
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
	struct hpsjam_packet_entry *pkt;

	if (1) {
		QMutexLocker locker(&lock);
		init();
	}

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

template <typename T>
void HpsJamSendPacket(T &s)
{
	struct hpsjam_packet_entry entry;
	float temp[2][HPSJAM_NOM_SAMPLES];

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
		entry.packet.put8Bit1ChSample(temp[0], HPSJAM_NOM_SAMPLES);
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
	s.output_pkt.send(s.address);
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
		/* for the future */
		s.in_audio[0].addSilence(HPSJAM_NOM_SAMPLES);
		s.in_audio[1].addSilence(HPSJAM_NOM_SAMPLES);
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
	uint16_t jitter;
	size_t num;

	input_pkt.recovery();

	/* update jitter */
	jitter = input_pkt.jitter.get_jitter_in_ms();
	in_audio[0].set_jitter_limit_in_ms(jitter);
	in_audio[1].set_jitter_limit_in_ms(jitter);

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
				if (ptr->getPing(packets, time_ms, passwd) &&
				    output_pkt.find(HPSJAM_TYPE_PING_REPLY) == 0) {
					pres = new struct hpsjam_packet_entry;
					pres->packet.setPing(0, time_ms, 0);
					pres->packet.type = HPSJAM_TYPE_PING_REPLY;
					pres->insert_tail(&output_pkt.head);
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
						QMutexLocker locker(&hpsjam_server_peers[x].lock);
						if (hpsjam_server_peers[x].valid == false)
							continue;
						QByteArray &t = hpsjam_server_peers[x].icon;
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
						QMutexLocker locker(&hpsjam_server_peers[x].lock);
						if (hpsjam_server_peers[x].valid == false)
							continue;
						t = hpsjam_server_peers[x].name.toUtf8();
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
						} else {
							QMutexLocker other(&hpsjam_server_peers[index + x].lock);
							pres->insert_tail(&hpsjam_server_peers[index + x].output_pkt.head);
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
						} else {
							QMutexLocker other(&hpsjam_server_peers[index + x].lock);
							pres->insert_tail(&hpsjam_server_peers[index + x].output_pkt.head);
						}
					}
				}
				break;
			case HPSJAM_TYPE_FADER_EQ_REQUEST:
				if (allow_mixer_access == false)
					break;
				if (ptr->getFaderData(mix, index, &data, num)) {
					if (mix != 0 || num <= 0)
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
					} else {
						QMutexLocker other(&hpsjam_server_peers[index].lock);
						pres->insert_tail(&hpsjam_server_peers[index].output_pkt.head);
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
		pres->packet.setPing(0, hpsjam_ticks, 0);
		pres->packet.type = HPSJAM_TYPE_PING_REQUEST;
		pres->insert_tail(&output_pkt.head);
	}

	/* extract samples for this tick */
	in_audio[0].remSamples(tmp_audio[0], HPSJAM_DEF_SAMPLES);
	in_audio[1].remSamples(tmp_audio[1], HPSJAM_DEF_SAMPLES);

	/* check if we should adjust the timer */
	hpsjam_server_adjust[in_audio[0].getLowWater()]++;

	/* clear output audio */
	memset(out_audio, 0, sizeof(out_audio));
}

void
hpsjam_server_peer :: audio_import()
{
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
	float temp[maxLevel][2];

	if (hpsjam_ticks % 128)
		return;

	for (unsigned x = 0; x != maxLevel; x++) {
		unsigned index = x + group * maxLevel;

		if (index >= hpsjam_num_server_peers) {
			temp[x][0] = 0.0f;
			temp[x][1] = 0.0f;
			continue;
		}

		QMutexLocker locker(&hpsjam_server_peers[index].lock);
		if (hpsjam_server_peers[index].valid) {
			temp[x][0] = hpsjam_server_peers[index].in_level[0].getLevel();
			temp[x][1] = hpsjam_server_peers[index].in_level[1].getLevel();
		} else {
			temp[x][0] = 0.0f;
			temp[x][1] = 0.0f;
		}
	}
	entry.packet.setFaderValue(0, group * maxLevel, temp[0], 2 * maxLevel);
	entry.packet.type = HPSJAM_TYPE_FADER_LEVEL_REPLY;
	hpsjam_server_broadcast(entry, 0, true);

	/* advance to next group */
	group++;
	if ((group * maxLevel) >= hpsjam_num_server_peers)
		group = 0;
}

Q_DECL_EXPORT void
hpsjam_server_tick()
{
	/* reset timer adjustment */
	memset(hpsjam_server_adjust, 0, sizeof(hpsjam_server_adjust));

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

	/* send out levels, if any */
	hpsjam_send_levels();

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
				for (unsigned z = 0; z != HPSJAM_DEF_SAMPLES; z++) {
					hpsjam_server_peers[x].out_audio[0][z] -=
					    hpsjam_server_peers[y].tmp_audio[0][z];
					hpsjam_server_peers[x].out_audio[1][z] -=
					    hpsjam_server_peers[y].tmp_audio[1][z];
				}
			} else {
				for (unsigned z = 0; z != HPSJAM_DEF_SAMPLES; z++) {
					hpsjam_server_peers[x].out_audio[0][z] +=
					    hpsjam_server_peers[y].tmp_audio[0][z];
					hpsjam_server_peers[x].out_audio[1][z] +=
					    hpsjam_server_peers[y].tmp_audio[1][z];
				}
			}
		}
		continue;
	do_solo:
		for (unsigned y = 0; y != hpsjam_num_server_peers; y++) {
			if (~hpsjam_server_peers[x].bits[y] & HPSJAM_BIT_SOLO)
				continue;
			if (hpsjam_server_peers[x].bits[y] & HPSJAM_BIT_INVERT) {
				for (unsigned z = 0; z != HPSJAM_DEF_SAMPLES; z++) {
					hpsjam_server_peers[x].out_audio[0][z] -=
					    hpsjam_server_peers[y].tmp_audio[0][z];
					hpsjam_server_peers[x].out_audio[1][z] -=
					    hpsjam_server_peers[y].tmp_audio[1][z];
				}
			} else {
				for (unsigned z = 0; z != HPSJAM_DEF_SAMPLES; z++) {
					hpsjam_server_peers[x].out_audio[0][z] +=
					    hpsjam_server_peers[y].tmp_audio[0][z];
					hpsjam_server_peers[x].out_audio[1][z] +=
					    hpsjam_server_peers[y].tmp_audio[1][z];
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

	if (address.valid() == false)
		return;

	const union hpsjam_frame *pkt;
	const struct hpsjam_packet *ptr;
	struct hpsjam_packet_entry *pres;
	union {
		float temp[HPSJAM_MAX_PKT];
		float audio[2][HPSJAM_DEF_SAMPLES];
	};
	size_t num;
	uint16_t jitter;

	input_pkt.recovery();

	/* update jitter */
	jitter = input_pkt.jitter.get_jitter_in_ms();
	out_audio[0].set_jitter_limit_in_ms(jitter);
	out_audio[1].set_jitter_limit_in_ms(jitter);

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
			uint64_t passwd;
			const char *data;
			uint8_t mix;
			uint8_t index;

			case HPSJAM_TYPE_PING_REQUEST:
				if (ptr->getPing(packets, time_ms, passwd) &&
				    output_pkt.find(HPSJAM_TYPE_PING_REPLY) == 0) {
					pres = new struct hpsjam_packet_entry;
					pres->packet.setPing(0, time_ms, 0);
					pres->packet.type = HPSJAM_TYPE_PING_REPLY;
					pres->insert_tail(&output_pkt.head);
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
					QByteArray t(data, num);
					if (mix != 0)
						break;
					if (self_index == -1) {
						self_index = index;
						emit receivedFaderSelf(mix, index);
					}
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
		pres->packet.setPing(0, hpsjam_ticks, 0);
		pres->packet.type = HPSJAM_TYPE_PING_REQUEST;
		pres->insert_tail(&output_pkt.head);
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

			if (hpsjam_client_peer->address.valid()) {
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
	}
}

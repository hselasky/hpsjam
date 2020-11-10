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

static void
hpsjam_server_broadcast(const struct hpsjam_packet_entry &entry,
    class hpsjam_server_peer *except)
{
	struct hpsjam_packet_entry *ptr;

	for (unsigned x = 0; x != hpsjam_num_server_peers; x++) {
		if (hpsjam_server_peers + x == except)
			continue;

		QMutexLocker locker(&hpsjam_server_peers[x].lock);

		if (hpsjam_server_peers[x].valid == false)
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
		    out_peak, left[x], right[x]);
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
			switch (ptr->type) {
			case HPSJAM_TYPE_AUDIO_8_BIT_1CH:
				num = ptr->get8Bit1ChSample(temp);
				assert(num <= HPSJAM_MAX_PKT);
				in_audio[0].addSamples(temp, num);
				in_audio[1].addSamples(temp, num);
				continue;
			case HPSJAM_TYPE_AUDIO_16_BIT_1CH:
				num = ptr->get16Bit1ChSample(temp);
				assert(num <= HPSJAM_MAX_PKT);
				in_audio[0].addSamples(temp, num);
				in_audio[1].addSamples(temp, num);
				continue;
			case HPSJAM_TYPE_AUDIO_24_BIT_1CH:
				num = ptr->get24Bit1ChSample(temp);
				assert(num <= HPSJAM_MAX_PKT);
				in_audio[0].addSamples(temp, num);
				in_audio[1].addSamples(temp, num);
				continue;
			case HPSJAM_TYPE_AUDIO_32_BIT_1CH:
				num = ptr->get32Bit1ChSample(temp);
				assert(num <= HPSJAM_MAX_PKT);
				in_audio[0].addSamples(temp, num);
				in_audio[1].addSamples(temp, num);
				continue;
			case HPSJAM_TYPE_AUDIO_8_BIT_2CH:
				num = ptr->get8Bit2ChSample(temp, temp + (HPSJAM_MAX_PKT / 2));
				assert(num <= (HPSJAM_MAX_PKT / 2));
				in_audio[0].addSamples(temp, num);
				in_audio[1].addSamples(temp + (HPSJAM_MAX_PKT / 2), num);
				continue;
			case HPSJAM_TYPE_AUDIO_16_BIT_2CH:
				num = ptr->get16Bit2ChSample(temp, temp + (HPSJAM_MAX_PKT / 2));
				assert(num <= (HPSJAM_MAX_PKT / 2));
				in_audio[0].addSamples(temp, num);
				in_audio[1].addSamples(temp + (HPSJAM_MAX_PKT / 2), num);
				continue;
			case HPSJAM_TYPE_AUDIO_24_BIT_2CH:
				num = ptr->get24Bit2ChSample(temp, temp + (HPSJAM_MAX_PKT / 2));
				assert(num <= (HPSJAM_MAX_PKT / 2));
				in_audio[0].addSamples(temp, num);
				in_audio[1].addSamples(temp + (HPSJAM_MAX_PKT / 2), num);
				continue;
			case HPSJAM_TYPE_AUDIO_32_BIT_2CH:
				num = ptr->get32Bit2ChSample(temp, temp + (HPSJAM_MAX_PKT / 2));
				assert(num <= (HPSJAM_MAX_PKT / 2));
				in_audio[0].addSamples(temp, num);
				in_audio[1].addSamples(temp + (HPSJAM_MAX_PKT / 2), num);
				continue;
			case HPSJAM_TYPE_AUDIO_32_BIT_2CH + 1 ... HPSJAM_TYPE_AUDIO_MAX:
				/* for the future */
				continue;
			case HPSJAM_TYPE_AUDIO_SILENCE:
				num = ptr->getSilence(temp);
				assert(num <= HPSJAM_MAX_PKT);
				in_audio[0].addSamples(temp, num);
				in_audio[1].addSamples(temp, num);
				continue;
			case HPSJAM_TYPE_ACK:
				/* check if other side received packet */
				if (ptr->getPeerSeqNo() == output_pkt.pend_seqno)
					output_pkt.advance();
				continue;
			default:
				break;
			}

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
	in_audio[0].remSamples(tmp_audio[0], HPSJAM_SAMPLE_RATE / 1000);
	in_audio[1].remSamples(tmp_audio[1], HPSJAM_SAMPLE_RATE / 1000);

	/* check if we should adjust the timer */
	switch (in_audio[0].getLowWater()) {
	case 0:
		hpsjam_timer_adjust++;	/* go slower */
		break;
	case 1:
		break;
	default:
		hpsjam_timer_adjust--;	/* go faster */
		break;
	}

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
		entry.packet.putSilence(HPSJAM_SAMPLE_RATE / 1000);
		output_pkt.append(entry);
		break;
	}

	/* send a packet */
	output_pkt.send(address);
}

Q_DECL_EXPORT void
hpsjam_server_tick()
{
	/* reset timer adjustment */
	hpsjam_timer_adjust = 0;

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
	uint16_t jitter;

	input_pkt.recovery();

	/* update jitter */
	jitter = input_pkt.jitter.get_jitter_in_ms();
	out_audio[0].set_jitter_limit_in_ms(jitter);
	out_audio[1].set_jitter_limit_in_ms(jitter);

	while ((pkt = input_pkt.first_pkt())) {
		for (ptr = pkt->start; ptr->valid(pkt->end); ptr = ptr->next()) {
			switch (ptr->type) {
			case HPSJAM_TYPE_AUDIO_8_BIT_1CH:
				num = ptr->get8Bit1ChSample(temp);
				assert(num <= HPSJAM_MAX_PKT);
				out_audio[0].addSamples(temp, num);
				out_audio[1].addSamples(temp, num);
				continue;
			case HPSJAM_TYPE_AUDIO_16_BIT_1CH:
				num = ptr->get16Bit1ChSample(temp);
				assert(num <= HPSJAM_MAX_PKT);
				out_audio[0].addSamples(temp, num);
				out_audio[1].addSamples(temp, num);
				continue;
			case HPSJAM_TYPE_AUDIO_24_BIT_1CH:
				num = ptr->get24Bit1ChSample(temp);
				assert(num <= HPSJAM_MAX_PKT);
				out_audio[0].addSamples(temp, num);
				out_audio[1].addSamples(temp, num);
				continue;
			case HPSJAM_TYPE_AUDIO_32_BIT_1CH:
				num = ptr->get32Bit1ChSample(temp);
				assert(num <= HPSJAM_MAX_PKT);
				out_audio[0].addSamples(temp, num);
				out_audio[1].addSamples(temp, num);
				continue;
			case HPSJAM_TYPE_AUDIO_8_BIT_2CH:
				num = ptr->get8Bit2ChSample(temp, temp + (HPSJAM_MAX_PKT / 2));
				assert(num <= (HPSJAM_MAX_PKT / 2));
				out_audio[0].addSamples(temp, num);
				out_audio[1].addSamples(temp + (HPSJAM_MAX_PKT / 2), num);
				continue;
			case HPSJAM_TYPE_AUDIO_16_BIT_2CH:
				num = ptr->get16Bit2ChSample(temp, temp + (HPSJAM_MAX_PKT / 2));
				assert(num <= (HPSJAM_MAX_PKT / 2));
				out_audio[0].addSamples(temp, num);
				out_audio[1].addSamples(temp + (HPSJAM_MAX_PKT / 2), num);
				continue;
			case HPSJAM_TYPE_AUDIO_24_BIT_2CH:
				num = ptr->get24Bit2ChSample(temp, temp + (HPSJAM_MAX_PKT / 2));
				assert(num <= (HPSJAM_MAX_PKT / 2));
				out_audio[0].addSamples(temp, num);
				out_audio[1].addSamples(temp + (HPSJAM_MAX_PKT / 2), num);
				continue;
			case HPSJAM_TYPE_AUDIO_32_BIT_2CH:
				num = ptr->get32Bit2ChSample(temp, temp + (HPSJAM_MAX_PKT / 2));
				assert(num <= (HPSJAM_MAX_PKT / 2));
				out_audio[0].addSamples(temp, num);
				out_audio[1].addSamples(temp + (HPSJAM_MAX_PKT / 2), num);
				continue;
			case HPSJAM_TYPE_AUDIO_32_BIT_2CH + 1 ... HPSJAM_TYPE_AUDIO_MAX:
				/* for the future */
				continue;
			case HPSJAM_TYPE_AUDIO_SILENCE:
				num = ptr->getSilence(temp);
				assert(num <= HPSJAM_MAX_PKT);
				out_audio[0].addSamples(temp, num);
				out_audio[1].addSamples(temp, num);
				continue;
			case HPSJAM_TYPE_ACK:
				/* check if other side received packet */
				if (ptr->getPeerSeqNo() == output_pkt.pend_seqno)
					output_pkt.advance();
				continue;
			default:
				break;
			}

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
					emit receivedFaderIcon(mix, index, new QByteArray(data, num));
				}
				break;
			case HPSJAM_TYPE_FADER_NAME_REPLY:
				if (ptr->getFaderData(mix, index, &data, num)) {
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
		pres->packet.setPing(0, hpsjam_ticks, 0);
		pres->packet.type = HPSJAM_TYPE_PING_REQUEST;
		pres->insert_tail(&output_pkt.head);
	}

	/* extract samples for this tick */
	in_audio[0].remSamples(audio[0], HPSJAM_SAMPLE_RATE / 1000);
	in_audio[1].remSamples(audio[1], HPSJAM_SAMPLE_RATE / 1000);

	/* check if we should adjust the timer */
	switch (in_audio[0].getLowWater()) {
	case 0:
		hpsjam_timer_adjust = 1;	/* go slower */
		break;
	case 1:
		hpsjam_timer_adjust = 0;
		break;
	default:
		hpsjam_timer_adjust = -1;	/* go faster */
		break;
	}

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
		entry.packet.putSilence(HPSJAM_SAMPLE_RATE / 1000);
		output_pkt.append(entry);
		break;
	}

	/* send a packet */
	output_pkt.send(address);
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

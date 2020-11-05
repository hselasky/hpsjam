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

#ifndef	_HPSJAM_PROTOCOL_H_
#define	_HPSJAM_PROTOCOL_H_

#include <QObject>

#include "socket.h"

#include <assert.h>

#include <stdint.h>
#include <string.h>

#include <sys/queue.h>

#define	HPSJAM_SEQ_MAX 16
#define	HPSJAM_MAX_PKT (255 * 4)
#define	HPSJAM_MAX_UDP 1400 /* bytes */

enum {
	HPSJAM_TYPE_END,
	HPSJAM_TYPE_AUDIO_8_BIT_1CH,
	HPSJAM_TYPE_AUDIO_8_BIT_2CH,
	HPSJAM_TYPE_AUDIO_16_BIT_1CH,
	HPSJAM_TYPE_AUDIO_16_BIT_2CH,
	HPSJAM_TYPE_AUDIO_24_BIT_1CH,
	HPSJAM_TYPE_AUDIO_24_BIT_2CH,
	HPSJAM_TYPE_AUDIO_32_BIT_1CH,
	HPSJAM_TYPE_AUDIO_32_BIT_2CH,
	HPSJAM_TYPE_AUDIO_MAX = 63,
	HPSJAM_TYPE_CONFIGURE_REQUEST,	
	HPSJAM_TYPE_PING_REQUEST,
	HPSJAM_TYPE_PING_REPLY,
	HPSJAM_TYPE_FADER_GAIN_REQUEST,
	HPSJAM_TYPE_FADER_GAIN_REPLY,
	HPSJAM_TYPE_FADER_PAN_REQUEST,
	HPSJAM_TYPE_FADER_PAN_REPLY,
	HPSJAM_TYPE_FADER_ICON_REQUEST,
	HPSJAM_TYPE_FADER_ICON_REPLY,
	HPSJAM_TYPE_FADER_NAME_REQUEST,
	HPSJAM_TYPE_FADER_NAME_REPLY,
	HPSJAM_TYPE_LYRICS_REQUEST,
	HPSJAM_TYPE_LYRICS_REPLY,
	HPSJAM_TYPE_CHAT_REQUEST,
	HPSJAM_TYPE_CHAT_REPLY,
};

struct hpsjam_header {
	uint8_t sequence;
	uint8_t getSeqNo() const {
		return (sequence % HPSJAM_SEQ_MAX);
	};
	uint8_t getRedNo() const {
		return ((sequence / HPSJAM_SEQ_MAX) % HPSJAM_SEQ_MAX);
	};
	void setSequence(uint8_t seq, uint8_t red) {
		sequence = (seq % HPSJAM_SEQ_MAX) +
		  ((red % HPSJAM_SEQ_MAX) * HPSJAM_SEQ_MAX);
	};
};

struct hpsjam_packet {
	uint8_t length;
	uint8_t type;
	uint8_t sequence[2];

	size_t getBytes() const {
		return (length * 4);
	};

	bool valid(const struct hpsjam_packet *end) const {
		const struct hpsjam_packet *ptr = this + length;
		if (type == HPSJAM_TYPE_END || length == 0 ||
		    (uintptr_t)ptr >= (uintptr_t)end)
			return (false);
		else
			return (true);
	}; 

	const struct hpsjam_packet *next() const {
		return (this + length);
	};

	int8_t getS8(size_t offset) const {
		return (int8_t)sequence[2 + offset];
	};
	void putS8(size_t offset, int8_t value) {
		sequence[2 + offset] = value;
	};
	int16_t getS16(size_t offset) const {
		return (int16_t)(sequence[2 + offset] | (sequence[2 + offset + 1] << 8));
	};
	void putS16(size_t offset, int16_t value) {
		sequence[2 + offset] = (uint8_t)value;
		sequence[2 + offset + 1] = (uint8_t)(value >> 8);
	};
	int32_t getS24(size_t offset) const {
		int temp =
		  (sequence[2 + offset]) |
		  (sequence[2 + offset + 1] << 8) |
		  (sequence[2 + offset + 2] << 16);
		if (temp & (1 << 23))
			temp |= -(1 << 23);
		return (temp);
	};
	void putS24(size_t offset, int32_t value) {
		sequence[2 + offset] = (uint8_t)value;
		sequence[2 + offset + 1] = (uint8_t)(value >> 8);
		sequence[2 + offset + 2] = (uint8_t)(value >> 16);
	};
	int32_t getS32(size_t offset) const {
		return (
		    (sequence[2 + offset]) |
		    (sequence[2 + offset + 1] << 8) |
		    (sequence[2 + offset + 2] << 16) |
		    (sequence[2 + offset + 3] << 24));
	};
	void putS32(size_t offset, int32_t value) {
		sequence[2 + offset] = (uint8_t)value;
		sequence[2 + offset + 1] = (uint8_t)(value >> 8);
		sequence[2 + offset + 2] = (uint8_t)(value >> 16);
		sequence[2 + offset + 3] = (uint8_t)(value >> 24);
	};

	size_t get8Bit2ChSample(float *left, float *right) const;
	size_t get16Bit2ChSample(float *left, float *right) const;
	size_t get24Bit2ChSample(float *left, float *right) const;
	size_t get32Bit2ChSample(float *left, float *right) const;

	size_t get8Bit1ChSample(float *left) const;
	size_t get16Bit1ChSample(float *left) const;
	size_t get24Bit1ChSample(float *left) const;
	size_t get32Bit1ChSample(float *left) const;

	void put8Bit2ChSample(float *left, float *right, size_t samples);
	void put16Bit2ChSample(float *left, float *right, size_t samples);
	void put24Bit2ChSample(float *left, float *right, size_t samples);
	void put32Bit2ChSample(float *left, float *right, size_t samples);

	void put8Bit1ChSample(float *left, size_t samples);
	void put16Bit1ChSample(float *left, size_t samples);
	void put24Bit1ChSample(float *left, size_t samples);
	void put32Bit1ChSample(float *left, size_t samples);

	uint8_t getLocalSeqNo() const {
		return (sequence[0]);
	};

	uint8_t getPeerSeqNo() const {
		return (sequence[1]);
	};

	void setLocalSeqNo(uint8_t seqno) {
		sequence[0] = seqno;
	};

	void setPeerSeqNo(uint8_t seqno) {
		sequence[1] = seqno;
	};

	bool getFaderGain(uint8_t &mix, uint8_t &index, float &gain) const {
		if (length >= 2) {
			mix = getS8(0);
			index = getS8(1);
			gain = (float)getS16(2) / (float)0x7fff;
			return (true);
		}
		return (false);
	};

	void setFaderGain(uint8_t mix, uint8_t index, float gain) {
		length = 2;
		sequence[0] = 0;
		sequence[1] = 0;
		putS8(0, mix);
		putS8(1, index);
		putS16(2, gain * 0x7fff);
	};

	bool getFaderPan(uint8_t &mix, uint8_t &index, float &gain) const {
		if (length >= 2) {
			mix = getS8(0);
			index = getS8(1);
			gain = (float)getS16(2) / (float)0x7fff;
			return (true);
		}
		return (false);
	};

	void setFaderPan(uint8_t mix, uint8_t index, float gain) {
		length = 2;
		sequence[0] = 0;
		sequence[1] = 0;
		putS8(0, mix);
		putS8(1, index);
		putS16(2, gain * 0x7fff);
	};

	bool getFaderIcon(uint8_t &mix, uint8_t &index, const char **pp) const {
		if (length >= 2) {
			mix = getS8(0);
			index = getS8(1);
			*pp = (char *)(sequence + 4);
			/* check for zero-termination */
			return (sequence[2 + (length - 1) * 4 - 1] == 0);
		}
		return (false);
	};

	void setFaderIcon(uint8_t &mix, uint8_t &index, const char *p) {
		size_t len = strlen(p) + 1;
		size_t tot = 2 + (len + 3 - 2) / 4;

		assert(tot <= 255);
		length = tot;
		sequence[0] = 0;
		sequence[1] = 0;
		putS8(0, mix);
		putS8(1, index);
		memcpy(sequence + 4, p, len);
	};

  	bool getFaderName(uint8_t &mix, uint8_t &index, const char **pp) const {
		if (length >= 2) {
			mix = getS8(0);
			index = getS8(1);
			*pp = (char *)(sequence + 4);
			/* check for zero-termination */
			return (sequence[2 + (length - 1) * 4 - 1] == 0);
		}
		return (false);
	};

	void setFaderName(uint8_t &mix, uint8_t &index, const char *p) {
		size_t len = strlen(p) + 1;
		size_t tot = 2 + (len + 3 - 2) / 4;

		assert(tot <= 255);
		length = tot;
		sequence[0] = 0;
		sequence[1] = 0;
		putS8(0, mix);
		putS8(1, index);
		memcpy(sequence + 4, p, len);
	};

  	bool getLyrics(const char **pp) const {
		if (length >= 2) {
			*pp = (char *)(sequence + 2);
			/* check for zero-termination */
			return (sequence[2 + (length - 1) * 4 - 1] == 0);
		}
		return (false);
	};

	void setLyrics(const char *p) {
		size_t len = strlen(p) + 1;
		size_t tot = 1 + (len + 3) / 4;

		assert(tot <= 255);
		length = tot;
		sequence[0] = 0;
		sequence[1] = 0;
		memcpy(sequence + 2, p, len);
	};

  	bool getChat(const char **pp) const {
		if (length >= 2) {
			*pp = (char *)(sequence + 2);
			/* check for zero-termination */
			return (sequence[2 + (length - 1) * 4 - 1] == 0);
		}
		return (false);
	};

	void setChat(const char *p) {
		size_t len = strlen(p) + 1;
		size_t tot = 1 + (len + 3) / 4;

		assert(tot <= 255);
		length = tot;
		sequence[0] = 0;
		sequence[1] = 0;
		memcpy(sequence + 2, p, len);
	};
  
	bool getConfigure(uint8_t &out_format) const {
		if (length >= 2) {
			out_format = getS8(0);
			return (true);
		}
		return (false);
	};

	void setConfigure(uint8_t out_format) {
		length = 2;
		sequence[0] = 0;
		sequence[1] = 0;
		putS8(0, out_format);
		putS8(1, 0);
		putS8(2, 0);
		putS8(3, 0);
	};

	bool getPing(uint16_t &packets, uint16_t &time_ms, uint64_t &passwd) const {
		if (length >= 4) {
			packets = getS16(0);
			time_ms = getS16(2);
			passwd = ((uint64_t)(uint32_t)getS32(4)) | (((uint64_t)(uint32_t)getS32(8)) << 32);
			return (true);
		}
		return (false);
	};

	void setPing(uint16_t packets, uint16_t time_ms, uint64_t passwd) {
		length = 4;
		sequence[0] = 0;
		sequence[1] = 0;
		putS16(0, packets);
		putS16(2, time_ms);
		putS32(4, (uint32_t)passwd);
		putS32(8, (uint32_t)(passwd >> 32));
	};
};

struct hpsjam_packet_entry;
typedef TAILQ_HEAD(, hpsjam_packet_entry) hpsjam_packet_head_t;

struct hpsjam_packet_entry {
	TAILQ_ENTRY(hpsjam_packet_entry) entry;
	union {
		struct hpsjam_packet packet;
		uint8_t raw[HPSJAM_MAX_PKT];
	};
	struct hpsjam_packet_entry & insert_tail(hpsjam_packet_head_t *phead)
	{
		TAILQ_INSERT_TAIL(phead, this, entry);
		return (*this);
	};
	struct hpsjam_packet_entry & insert_head(hpsjam_packet_head_t *phead)
	{
		TAILQ_INSERT_HEAD(phead, this, entry);
		return (*this);
	};
	struct hpsjam_packet_entry & remove(hpsjam_packet_head_t *phead)
	{
		TAILQ_REMOVE(phead, this, entry);
		return (*this);
	};
};

union hpsjam_frame {
	uint8_t raw[HPSJAM_MAX_UDP];
  	struct {
		struct hpsjam_header hdr;
		struct hpsjam_packet start[(HPSJAM_MAX_UDP - sizeof(hdr)) / sizeof(hpsjam_packet)];
		struct hpsjam_packet end[0];
	};
	void clear() {
		memset(this, 0, sizeof(*this));
	};
	void do_xor(const union hpsjam_frame &other) {
		for (size_t x = 0; x != sizeof(this); x++)
			raw[x] ^= other.raw[x];
	};
};

class hpsjam_output_packetizer : public QObject {
	Q_OBJECT;
public:
	union hpsjam_frame current;
	union hpsjam_frame mask;
	hpsjam_packet_head_t head;
	struct hpsjam_packet_entry *pending;
	uint16_t pend_count; /* pending tick count */
	uint8_t pend_seqno; /* pending sequence number */
	uint8_t peer_seqno; /* peer sequence number */
	uint8_t d_cur;	/* current distance between XOR frames */
	uint8_t d_max;	/* maximum distance between XOR frames */
	uint8_t seqno;	/* current sequence number */
	size_t offset;	/* current data offset */
	size_t d_len;	/* maximum XOR frame length */

	hpsjam_output_packetizer() {
		TAILQ_INIT(&head);
		pending = 0;
		init();
	};

	bool empty() const {
		return (TAILQ_FIRST(&head) == 0);
	};

	void init(uint8_t distance = 2) {
		struct hpsjam_packet_entry *pkt;
		d_cur = 0;
		d_max = distance % HPSJAM_SEQ_MAX;
		pend_count = 0;
		pend_seqno = 0;
		peer_seqno = 0;
		seqno = 0;
		offset = 0;
		current.clear();
		mask.clear();

		while ((pkt = TAILQ_FIRST(&head))) {
			pkt->remove(&head);
			delete pkt;
		}

		delete pending;
		pending = 0;
	};

	bool append(const struct hpsjam_packet_entry &entry)
	{
		size_t remainder = sizeof(current) - sizeof(current.hdr) - offset;
		size_t len = entry.packet.getBytes();

		if (len <= remainder) {
			memcpy(current.raw + sizeof(current.hdr) + offset, entry. raw, len);
			offset += len;
			return (true);
		}
		return (false);
	};

	void advance() {
		delete pending;
		pending = 0;
	};

	void send(const struct hpsjam_socket_address &addr) {
		if (d_cur == d_max) {
			/* finalize XOR packet */
			mask.hdr.setSequence(seqno, d_max);
			addr.sendto((const char *)&mask, d_len + sizeof(current.hdr));
			mask.clear();
			d_cur = 0;
			d_len = 0;
		} else {
			/* add a control packet, if possible */
			if (pending == 0) {
				pending = TAILQ_FIRST(&head);
				if (pending != 0) {
					pending->remove(&head);
					pending->packet.setLocalSeqNo(pend_seqno);
					pending->packet.setPeerSeqNo(peer_seqno);
					pend_count = 0;
					pend_seqno++;
					append(*pending);
				} else {
					pend_count++;
				}
			} else {
				pend_count++;
				if ((pend_count % 64) == 0) {
					pending->packet.setPeerSeqNo(peer_seqno);
					append(*pending);
				}
			}
			if (pend_count == 1000)
				emit pendingWatchdog();
			else if (pend_count == 2000)
				emit pendingTimeout();

			current.hdr.setSequence(seqno, 0);
			addr.sendto((const char *)&current, offset + sizeof(current.hdr));
			mask.do_xor(current);
			current.clear();
			seqno++;
			d_cur++;
			/* keep track of maximum XOR length */
			if (d_len < offset)
				d_len = offset;
			offset = 0;
		}
	};
signals:
	void pendingWatchdog();
	void pendingTimeout();
};

struct hpsjam_input_packetizer {
	union hpsjam_frame current[HPSJAM_SEQ_MAX];
	union hpsjam_frame mask[HPSJAM_SEQ_MAX];
	uint8_t valid[HPSJAM_SEQ_MAX];
	float histogram[HPSJAM_SEQ_MAX];
	uint8_t seqno;
	uint8_t jitter;	

	void init() {
		for (size_t x = 0; x != HPSJAM_SEQ_MAX; x++) {
			current[x].clear();
			mask[x].clear();
		}
		memset(valid, 0, sizeof(valid));
		memset(histogram, 0, sizeof(histogram));
		seqno = 0;
		jitter = 0;
	};

	const union hpsjam_frame *first_pkt() {
		unsigned mask = 0;
		unsigned start;
		unsigned min_x;
		unsigned pkts;

		for (uint8_t x = pkts = 0; x != HPSJAM_SEQ_MAX; x++) {
			if (valid[x] & 1) {
				mask |= (1 << x);
				pkts++;
			}
			if (valid[x] & 2) {
				pkts++;
			}
		}

		start = mask;
		for (uint8_t x = min_x = 0; x != HPSJAM_SEQ_MAX; x++) {
			if (start > mask) {
				start = mask;
				min_x = x;
			}
			if (mask & 1)
				mask |= 1 << HPSJAM_SEQ_MAX;
			mask >>= 1;
		}

		/*
		 * Consume if there are tree consequtive valid
		 * packets, or if jitter amount is exceeded:
		 */
		if (((start & 7) == 7) || (start != 0 && pkts >= jitter)) {
			assert(valid[min_x] != 0);
			valid[min_x] = 0;
			return (current + min_x);
		}
		return (0);
	};
	void recovery() {
		for (uint8_t x = 0; x != HPSJAM_SEQ_MAX; x++) {
			if (~valid[x] & 2)
				continue;
			const uint8_t rx_red = mask[x].hdr.getRedNo();
			uint8_t rx_missing = 0;
			for (uint8_t y = 0; y != rx_red; y++) {
				const uint8_t z = (HPSJAM_SEQ_MAX + x - y - 1);
				rx_missing += (~valid[z] & 1);
			}
			if (rx_missing == 1) {
				/* one frame missing and we have the XOR frame */
				for (uint8_t y = 0; y != rx_red; y++) {
					const uint8_t z = (HPSJAM_SEQ_MAX + x - y - 1);
					if (valid[z] & 1)
						mask[x].do_xor(current[z]);
				}
				for (uint8_t y = 0; y != rx_red; y++) {
					const uint8_t z = (HPSJAM_SEQ_MAX + x - y - 1);
					if (~valid[z] & 1)
						current[z] = mask[x];
				}
				valid[x] &= ~2;
			} else if (rx_missing == 0) {
				valid[x] &= ~2;
			}
		}
	};

	void receive(const union hpsjam_frame &frame) {
		const uint8_t rx_seqno = frame.hdr.getSeqNo();
		const uint8_t rx_red = frame.hdr.getRedNo();

		if (rx_red != 0) {
			mask[rx_seqno] = frame;
			valid[rx_seqno] |= 2;
		} else {
			current[rx_seqno] = frame;
			valid[rx_seqno] |= 1;
		}

		const uint8_t index = (HPSJAM_SEQ_MAX + rx_seqno - seqno) % HPSJAM_SEQ_MAX;

		histogram[index] += 1.0f;

		if (histogram[index] >= HPSJAM_SEQ_MAX) {
			uint8_t x, xmax;

			for (x = 0; x != HPSJAM_SEQ_MAX; x++)
				histogram[x] /= 2.0f;

			/* find width of peak */
			for (x = xmax = 1; x != (HPSJAM_SEQ_MAX - 1) / 2; x++) {
				if (histogram[(index + x) % HPSJAM_SEQ_MAX] >= 0.5f ||
				    histogram[(HPSJAM_SEQ_MAX + index - x) % HPSJAM_SEQ_MAX] >= 0.5f) {
					xmax = x + 1;
				}
			}

			/* update jitter setting */
			jitter = 2 * xmax;
		}
	};
};

#endif		/* _HPSJAM_PROTOCOL_H_ */

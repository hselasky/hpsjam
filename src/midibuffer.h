/*-
 * Copyright (c) 2021 Hans Petter Selasky. All rights reserved.
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

#ifndef	_HPSJAM_MIDIBUFFER_H_
#define	_HPSJAM_MIDIBUFFER_H_

#include <assert.h>
#include <string.h>

#include "hpsjam.h"

struct hpsjam_midi_parse {
	void clear() {
		memset(this, 0, sizeof(*this));
	};
	uint8_t *temp_cmd;
	uint8_t	temp_0[4];
	uint8_t	temp_1[4];
	uint8_t	state;
#define	HPSJAM_MIDI_ST_UNKNOWN   0		/* scan for command */
#define	HPSJAM_MIDI_ST_1PARAM    1
#define	HPSJAM_MIDI_ST_2PARAM_1  2
#define	HPSJAM_MIDI_ST_2PARAM_2  3
#define	HPSJAM_MIDI_ST_SYSEX_0   4
#define	HPSJAM_MIDI_ST_SYSEX_1   5
#define	HPSJAM_MIDI_ST_SYSEX_2   6
};

class hpsjam_midi_buffer {
public:
	enum { MIDI_BUFFER_MAX = 256 };

	uint8_t data[MIDI_BUFFER_MAX];
	size_t consumer;
	size_t total;

	void clear() {
		memset(data, 0, sizeof(data));
		consumer = 0;
		total = 0;
	};

	hpsjam_midi_buffer() {
		clear();
	};

	size_t remData(uint8_t *dst, size_t num) {
		uint8_t *old = dst;
		size_t fwd = MIDI_BUFFER_MAX - consumer;

		/* check for maximum amount of data that can be removed */
		if (num > total)
			num = total;

		/* copy samples from ring-buffer */
		while (num != 0) {
			if (fwd > num)
				fwd = num;
			memcpy(dst, data + consumer, sizeof(data[0]) * fwd);
			dst += fwd;
			num -= fwd;
			consumer += fwd;
			total -= fwd;
			if (consumer == MIDI_BUFFER_MAX) {
				consumer = 0;
				fwd = MIDI_BUFFER_MAX;
			} else {
				break;
			}
		}
		return (dst - old);
	};

	void addData(const uint8_t *src, size_t num) {
		size_t producer = (consumer + total) % MIDI_BUFFER_MAX;
		size_t fwd = MIDI_BUFFER_MAX - producer;
		size_t max = MIDI_BUFFER_MAX - total;

		if (num > max)
			num = max;

		/* copy samples to ring-buffer */
		while (num != 0) {
			if (fwd > num)
				fwd = num;
			if (fwd != 0) {
				memcpy(data + producer, src, sizeof(data[0]) * fwd);

				/* update last sample */
				src += fwd;
				num -= fwd;
				total += fwd;
				producer += fwd;
			}
			if (producer == MIDI_BUFFER_MAX) {
				producer = 0;
				fwd = MIDI_BUFFER_MAX;
			} else {
				break;
			}
		}
	};
};

#endif		/* _HPSJAM_MIDIBUFFER_H_ */

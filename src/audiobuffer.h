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

#ifndef	_HPSJAM_AUDIOBUFFER_H_
#define	_HPSJAM_AUDIOBUFFER_H_

#include "hpsjam.h"
#include "protocol.h"

#define	HPSJAM_MAX_SAMPLES \
	(HPSJAM_SEQ_MAX * 2 * (HPSJAM_SAMPLE_RATE / 1000))	/* samples */

class hpsjam_audio_buffer {
public:
	float samples[HPSJAM_MAX_SAMPLES];
	float stats[HPSJAM_SEQ_MAX * 2];
	float last_sample;
	size_t consumer;
	size_t total;

	void clear() {
		memset(this, 0, sizeof(*this));
	};

	hpsjam_audio_buffer() {
		clear();
	};

	/* remove samples from buffer, must be called periodically */
	void remSamples(float *dst, size_t num) {
		size_t fwd = HPSJAM_MAX_SAMPLES - consumer;
		uint8_t index = total / (HPSJAM_SAMPLE_RATE / 1000);

		stats[index] += 1.0f;

		if (stats[index] >= HPSJAM_SEQ_MAX * 2) {
			for (uint8_t x = 0; x != HPSJAM_SEQ_MAX * 2; x++)
				stats[x] /= 2.0f;
		}

		/* fill missing samples with last value */
		if (total < num) {
			for (size_t x = total; x != num; x++)
				dst[x] = last_sample;
			num = total;
		}

		/* copy samples from ring-buffer */
		while (num != 0) {
			if (fwd > num)
				fwd = num;
			memcpy(dst, samples + consumer, sizeof(samples[0]) * fwd);
			dst += fwd;
			num -= fwd;
			consumer += fwd;
			total -= fwd;
			if (consumer == HPSJAM_MAX_SAMPLES) {
				consumer = 0;
				fwd = HPSJAM_MAX_SAMPLES;
			} else {
				break;
			}
		}
	};

	/* add samples to buffer */
	void addSamples(const float *src, size_t num) {
		size_t producer = (consumer + total) % HPSJAM_MAX_SAMPLES;
		size_t fwd = HPSJAM_MAX_SAMPLES - producer;
		size_t max = HPSJAM_MAX_SAMPLES - total;

		if (num > max)
			num = max;

		/* copy samples to ring-buffer */
		while (num != 0) {
			if (fwd > num)
				fwd = num;
			if (fwd != 0) {
				last_sample = src[fwd - 1];
				memcpy(samples + producer, src, sizeof(samples[0]) * fwd);
				src += fwd;
				num -= fwd;
				total += fwd;
			}
			if (producer == HPSJAM_MAX_SAMPLES) {
				producer = 0;
				fwd = HPSJAM_MAX_SAMPLES;
			} else {
				break;
			}
		}
	};

	/* grow ring-buffer */
	void grow() {
		const size_t p[2] =
		  { (consumer + total + HPSJAM_MAX_SAMPLES - 1) % HPSJAM_MAX_SAMPLES,
		    (consumer + total + HPSJAM_MAX_SAMPLES - 2) % HPSJAM_MAX_SAMPLES };
		const float append = samples[p[0]];
		samples[p[0]] = (samples[p[0]] + samples[p[1]]) / 2.0f;
		addSamples(&append, 1);
	};

	/* shrink ring-buffer */
	void shrink() {
		const size_t p[2] =
		  { (consumer + total + HPSJAM_MAX_SAMPLES - 1) % HPSJAM_MAX_SAMPLES,
		    (consumer + total + HPSJAM_MAX_SAMPLES - 2) % HPSJAM_MAX_SAMPLES };
		if (total > 1) {
			samples[p[1]] = (samples[p[0]] + samples[p[1]]) / 2.0f;
			total--;
		}
	};
};

#endif		/* _HPSJAM_AUDIOBUFFER_H_ */

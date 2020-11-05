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

#include <math.h>

#include "hpsjam.h"
#include "protocol.h"

#define	HPSJAM_MAX_SAMPLES \
	(HPSJAM_SEQ_MAX * 2 * (HPSJAM_SAMPLE_RATE / 1000))	/* samples */

static inline float
level_encode(float value)
{
	float divisor = logf(1.0f + 255.0f);

	if (value == 0.0f)
		return (0);
	else if (value < 0.0f)
		return - (logf(1.0f - 255.0f * value) / divisor);
	else
		return (logf(1.0f + 255.0f * value) / divisor);
}

static inline float
level_decode(float value)
{
	constexpr float multiplier = (1.0f / 255.0f);

	if (value == 0.0f)
		return (0);
	else if (value < 0.0f)
		return - multiplier * (powf(1.0f + 255.0f, -value) - 1.0f);
	else
		return multiplier * (powf(1.0f + 255.0f, value) - 1.0f);
}

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

	float getMaxLevel() const {
		size_t fwd = HPSJAM_MAX_SAMPLES - consumer;
		float level = 0.0f;

		if (fwd >= total) {
			for (size_t x = 0; x != total; x++) {
				const float v = fabsf(samples[consumer + x]);
				if (v > level)
					level = v;
			}
		} else {
			for (size_t x = 0; x != fwd; x++) {
				const float v = fabsf(samples[consumer + x]);
				if (v > level)
					level = v;
			}

			/* buffer wrap around */
			for (size_t x = 0; x != (total - fwd); x++) {
				const float v = fabsf(samples[x]);
				if (v > level)
					level = v;
			}
		}
		return (level);
	};

	uint8_t getLowWater() const {
		for (uint8_t x = 0; x != HPSJAM_SEQ_MAX * 2; x++) {
			if (stats[x] >= 0.5f)
				return (x);
		}
		return (HPSJAM_SEQ_MAX * 2);
	};

	/* remove samples from buffer, must be called periodically */
	void remSamples(float *dst, size_t num) {
		size_t fwd = HPSJAM_MAX_SAMPLES - consumer;

		/* fill missing samples with last value */
		if (total < num) {
			for (size_t x = total; x != num; x++) {
				dst[x] = last_sample;
				last_sample -= last_sample / HPSJAM_SAMPLE_RATE;
			}
			num = total;
		}

		/* keep track of low water mark */
		const uint8_t index = (total - num) / (HPSJAM_SAMPLE_RATE / 1000);

		stats[index] += 1.0f;

		if (stats[index] >= HPSJAM_SEQ_MAX * 2) {
			for (uint8_t x = 0; x != HPSJAM_SEQ_MAX * 2; x++)
				stats[x] /= 2.0f;

			const uint8_t low = getLowWater();

			/*
			 * Grow or shrink the buffer depending on the
			 * amount of supplied data:
			 */
			if (total < HPSJAM_MAX_SAMPLES && low < 1)
				grow();
			else if (total > num && low > 2)
				shrink();
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
		if (total > 1) {
			const float append = samples[p[0]];
			samples[p[0]] = (samples[p[0]] + samples[p[1]]) / 2.0f;
			addSamples(&append, 1);
		}
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

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

#ifndef	_HPSJAM_AUDIOBUFFER_H_
#define	_HPSJAM_AUDIOBUFFER_H_

#include <math.h>
#include <assert.h>

#include "hpsjam.h"
#include "protocol.h"

#define	HPSJAM_MAX_SAMPLES \
	(32 * HPSJAM_DEF_SAMPLES) /* 32 ms */

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

class hpsjam_audio_level {
public:
	float level;

	hpsjam_audio_level() {
		clear();
	};
	void clear() {
		level = 0;
	};
	void addSamples(const float *ptr, size_t num) {
		for (size_t x = 0; x != num; x++) {
			const float v = fabsf(ptr[x]);
			if (v > level)
				level = v;
		}
		if (level > 1.0f)
			level = 1.0f;
	};
	float getLevel() {
		float retval = level;
		level = retval / 2.0f;
		return (retval);
	};
};

class hpsjam_audio_buffer {
	enum { fadeSamples = HPSJAM_DEF_SAMPLES };
public:
	float samples[HPSJAM_MAX_SAMPLES];
	float last_sample;
	size_t consumer;
	size_t total;
	uint16_t fade_in;
	uint16_t low_water;
	uint16_t high_water;
	uint16_t target_water;

	void doWater() {
		if (low_water > total)
			low_water = total;
		if (high_water < total)
			high_water = total;
	};

	void clear() {
		memset(samples, 0, sizeof(samples));
		last_sample = 0;
		consumer = 0;
		total = 0;
		fade_in = fadeSamples;
		low_water = 65535;
		high_water = 0;
	};

	hpsjam_audio_buffer() {
		clear();
		target_water = HPSJAM_MAX_SAMPLES / 2;
	};

	int setWaterTarget(int value) {

		value *= HPSJAM_DEF_SAMPLES;

		/* range check */
		if (value > (HPSJAM_MAX_SAMPLES / 2))
			value = HPSJAM_MAX_SAMPLES / 2;
		else if (value < (4 * HPSJAM_DEF_SAMPLES))
			value = 4 * HPSJAM_DEF_SAMPLES;
		/* set new value */
		target_water = value;

		return (value / HPSJAM_DEF_SAMPLES);
	};

	int getWaterRef() const {
		if (low_water > high_water)
			return (0);	/* normal */
		size_t diff = high_water - low_water;
		ssize_t middle = low_water + (diff / 2) - (ssize_t)target_water;
		return (middle);
	};

	/* getLowWater() returns one of 0,1 or 2. */
	uint8_t getLowWater() const {
		const int ref = getWaterRef();

		if (ref < 0)
			return (0);	/* low data - go slower */
		else if (ref > 0)
			return (2);	/* high data - go faster */
		else
			return (1);	/* normal */
	};

	/* remove samples from buffer, must be called periodically */
	void remSamples(float *dst, size_t num) {
		size_t fwd;

		doWater();

		/* fill missing samples with last value, if any */
		if (num > total) {
			for (size_t x = total; x != num; x++) {
				last_sample -= last_sample / HPSJAM_SAMPLE_RATE;
				dst[x] = last_sample;
			}
			num = total;
			fade_in = fadeSamples;
		}

		/* setup forward size */
		fwd = HPSJAM_MAX_SAMPLES - consumer;

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
				assert(num == 0);
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
				/* check if there was a discontinuity, and fade in audio */
				if (fade_in != 0) {
					for (size_t x = 0; x != fwd; x++) {
						const float f = (float)fade_in / (float)fadeSamples;
						last_sample -= last_sample / HPSJAM_SAMPLE_RATE;
						samples[producer + x] = src[x] - f * src[x] + last_sample * f;
						fade_in -= (fade_in != 0);
					}
				} else {
					memcpy(samples + producer, src, sizeof(samples[0]) * fwd);
				}
				/* update last sample */
				last_sample = samples[producer + fwd - 1];
				src += fwd;
				num -= fwd;
				total += fwd;
				producer += fwd;
			}
			if (producer == HPSJAM_MAX_SAMPLES) {
				producer = 0;
				fwd = HPSJAM_MAX_SAMPLES;
			} else {
				assert(num == 0);
				break;
			}
		}
	};

	/* add silence to buffer */
	void addSilence(size_t num) {
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
				for (size_t x = 0; x != fwd; x++) {
					last_sample -= last_sample / HPSJAM_SAMPLE_RATE;
					samples[producer + x] = last_sample;
				}
				fade_in = fadeSamples;
				num -= fwd;
				total += fwd;
				producer += fwd;
			}
			if (producer == HPSJAM_MAX_SAMPLES) {
				producer = 0;
				fwd = HPSJAM_MAX_SAMPLES;
			} else {
				assert(num == 0);
				break;
			}
		}
	};
	void adjustBuffer();
};

#endif		/* _HPSJAM_AUDIOBUFFER_H_ */

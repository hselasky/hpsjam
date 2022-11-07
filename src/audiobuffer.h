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
	float ping_pong_data[fadeSamples];
	float last_sample;
	size_t ping_pong_offset;
	size_t consumer;
	size_t total;
	uint16_t fade_in;
	uint16_t low_water;
	uint16_t high_water;
	uint16_t target_water;

	void doWater(size_t num) {
		if (num > total)
			low_water = 0;
		else if (low_water > total - num)
			low_water = total - num;

		if (high_water < total)
			high_water = total;
	};

	void clear() {
		memset(samples, 0, sizeof(samples));
		memset(ping_pong_data, 0, sizeof(ping_pong_data));
		ping_pong_offset = 0;
		last_sample = 0;
		consumer = 0;
		total = 0;
		fade_in = fadeSamples;
		low_water = HPSJAM_MAX_SAMPLES;
		high_water = 0;
	};

	void addPingPongBuffer(float sample) {
		ping_pong_data[ping_pong_offset] = sample;
		if (++ping_pong_offset == fadeSamples)
			ping_pong_offset = 0;
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
		ssize_t diff = (high_water - low_water) / 2;
		/* too much noise, allow bigger buffer */
		if (diff > target_water)
			return (0);
		/* try to fit the buffer at the target level */
		ssize_t middle = low_water + diff - (ssize_t)target_water;
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

	void adjustBuffer();
	void remSamples(float *, size_t);
	void addSamples(const float *, size_t);
	void addSilence(size_t);
};

#endif		/* _HPSJAM_AUDIOBUFFER_H_ */

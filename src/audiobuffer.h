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
	enum {
		WATER_LOW = 0,
		WATER_NORMAL = 1,
		WATER_HIGH = 2,
		WATER_MAX = 32,
	};
	float samples[HPSJAM_MAX_SAMPLES];
	float ping_pong_data[fadeSamples];
	float last_sample;
	size_t consumer;
	size_t total;
	uint16_t fade_in;
	uint16_t ping_pong_offset;
	uint16_t target_water;
	uint16_t water_index;
	uint16_t last_water[WATER_MAX];
	uint16_t high_water;
	uint16_t low_water;
	bool adjust_buffer;

	void addWater(uint16_t level) {
		uint16_t &previous = last_water[water_index % WATER_MAX];
		bool recompute = (previous == low_water || previous == high_water);
		previous = level;
		water_index++;

		/* only recompute water levels when really needed */
		if (recompute) {
			low_water = HPSJAM_MAX_SAMPLES;
			high_water = 0;

			for (unsigned x = 0; x != WATER_MAX; x++) {
				if (low_water > last_water[x])
					low_water = last_water[x];
				if (high_water < last_water[x])
					high_water = last_water[x];
			}
		} else {
			if (low_water > level)
				low_water = level;
			else if (high_water < level)
				high_water = level;
		}
	};

	void doWater(size_t num) {
		if (last_water[(water_index + WATER_MAX - 1) % WATER_MAX] != total)
			addWater(total);
		if (num > total)
			addWater(0);
		else if (num != 0)
			addWater(total - num);
	};

	void clear() {
		memset(samples, 0, sizeof(samples));
		memset(ping_pong_data, 0, sizeof(ping_pong_data));
		ping_pong_offset = 0;
		last_sample = 0;
		consumer = 0;
		total = 0;
		fade_in = fadeSamples;
		memset(last_water, 0, sizeof(last_water));
		water_index = 0;
		high_water = 0;
		low_water = HPSJAM_MAX_SAMPLES;
		adjust_buffer = false;
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
		int slack = high_water - low_water;
		if (slack < 0)
			return (0);	/* not ready */

		/* limit slack by target_water */
		if (slack < target_water)
			slack = target_water;

		/* use a half buffer length for slack */
		slack /= 2;

		if (low_water < slack) {
			int max_adjust = high_water - HPSJAM_MAX_SAMPLES;
			int cur_adjust = low_water - slack;

			if (max_adjust > cur_adjust)
				return (max_adjust);
			else
				return (cur_adjust);
		} else if (high_water > 3 * slack + 2 * HPSJAM_DEF_SAMPLES) {
			int max_adjust = low_water - slack;
			int cur_adjust = high_water - 2 * slack;

			if (max_adjust > cur_adjust)
				return (cur_adjust);
			else
				return (max_adjust);
		} else {
			return (0);	/* don't touch */
		}
	};

	uint8_t getLowWater() const {
		int slack = high_water - low_water;
		if (slack < 0)
			return (WATER_NORMAL);	/* not ready */

		/* limit slack by target_water */
		if (slack < target_water)
			slack = target_water;

		/* use a half buffer length for slack */
		slack /= 2;

		if (low_water < slack + HPSJAM_DEF_SAMPLES)
			return (WATER_LOW);
		else if (low_water > 3 * slack + HPSJAM_DEF_SAMPLES)
			return (WATER_HIGH);
		else
			return (WATER_NORMAL);
	};

	void adjustBuffer() {
		adjust_buffer = true;
	};
	void doAdjustBuffer(int);
	void remSamples(float *, size_t);
	void addSamples(const float *, size_t);
	void addSilence(size_t);
};

#endif		/* _HPSJAM_AUDIOBUFFER_H_ */

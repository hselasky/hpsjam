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

#include <assert.h>
#include <math.h>

#include "protocol.h"

/* https://en.wikipedia.org/wiki/M-law_algorithm */

static float hpsjam_powf[4][256];
static float hpsjam_mul_8;
static float hpsjam_mul_16;
static float hpsjam_mul_24;
static float hpsjam_mul_32;

static void __attribute__((__constructor__))
audio_init(void)
{
	for (uint8_t x = 0; x != 4; x++) {
		for (uint32_t y = 0; y != 256; y++) {
			hpsjam_powf[x][y] =
			    powf(1.0f + 255.0f, (float)y / 128.0f /
			        (float)(1U << (8 * x)));
		}
	}

	hpsjam_mul_8 = 127.0f / logf(1.0f + 255.0f);
	hpsjam_mul_16 = 32767.0f / logf(1.0f + 255.0f);
	hpsjam_mul_24 = 8388607.0f / logf(1.0f + 255.0f);
	hpsjam_mul_32 = 2147483647.0f / logf(1.0f + 255.0f);
}

static inline int
audio_encode(float value, float multiplier)
{
	if (value == 0.0f)
		return (0);
	else if (value < 0.0f)
		return - (logf(1.0f - 255.0f * value) * multiplier);
	else
		return (logf(1.0f + 255.0f * value) * multiplier);
}

static inline float
audio_decode(int input, const float scale)
{
	constexpr float multiplier = (1.0f / 255.0f);

	if (input == 0) {
		return (0.0f);
	} else if (input < 0) {
		const uint32_t value = - input * scale;
		return - multiplier * (
		    hpsjam_powf[0][(uint8_t)(value >> 24)] *
		    hpsjam_powf[1][(uint8_t)(value >> 16)] *
		    hpsjam_powf[2][(uint8_t)(value >> 8)] *
		    hpsjam_powf[3][(uint8_t)value] - 1.0f);
	} else {
		const uint32_t value = input * scale;
		return multiplier * (
		    hpsjam_powf[0][(uint8_t)(value >> 24)] *
		    hpsjam_powf[1][(uint8_t)(value >> 16)] *
		    hpsjam_powf[2][(uint8_t)(value >> 8)] *
		    hpsjam_powf[3][(uint8_t)value] - 1.0f);
	}
}

size_t
hpsjam_packet::get8Bit2ChSample(float *left, float *right) const
{
	const size_t samples = (length - 1) * 2;

	for (size_t x = 0; x != samples; x++) {
		left[x] = audio_decode(getS8(x * 2), (float)(1U << 31) / 127.0f);
		right[x] = audio_decode(getS8(x * 2 + 1), (float)(1U << 31) / 127.0f);
	}
	return (samples);
}

size_t
hpsjam_packet::get16Bit2ChSample(float *left, float *right) const
{
	const size_t samples = (length - 1);

	for (size_t x = 0; x != samples; x++) {
		left[x] = audio_decode(getS16(x * 4), (float)(1U << 31) / 32767.0f);
		right[x] = audio_decode(getS16(x * 4 + 2), (float)(1U << 31) / 32767.0f);
	}
	return (samples);
}

size_t
hpsjam_packet::get24Bit2ChSample(float *left, float *right) const
{
	const size_t samples = ((length - 1) * 4) / 6;

	for (size_t x = 0; x != samples; x++) {
		left[x] = audio_decode(getS24(x * 6), (float)(1U << 31) / 8388607.0f);
		right[x] = audio_decode(getS24(x * 6 + 3), (float)(1U << 31) / 8388607.0f);
	}
	return (samples);
}

size_t
hpsjam_packet::get32Bit2ChSample(float *left, float *right) const
{
	const size_t samples = (length - 1) / 2;

	for (size_t x = 0; x != samples; x++) {
		left[x] = audio_decode(getS32(x * 8), (float)(1U << 31) / 2147483647.0f);
		right[x] = audio_decode(getS32(x * 8 + 4), (float)(1U << 31) / 2147483647.0f);
	}
	return (samples);
}

size_t
hpsjam_packet::get8Bit1ChSample(float *left) const
{
	const size_t samples = (length - 1) * 4;

	for (size_t x = 0; x != samples; x++) {
		left[x] = audio_decode(getS8(x), (float)(1U << 31) / 127.0f);
	}
	return (samples);
}

size_t
hpsjam_packet::get16Bit1ChSample(float *left) const
{
	const size_t samples = (length - 1) * 2;

	for (size_t x = 0; x != samples; x++) {
		left[x] = audio_decode(getS16(2 * x), (float)(1U << 31) / 32767.0f);
	}
	return (samples);
}

size_t
hpsjam_packet::get24Bit1ChSample(float *left) const
{
	const size_t samples = ((length - 1) * 4) / 3;

	for (size_t x = 0; x != samples; x++) {
		left[x] = audio_decode(getS24(3 * x), (float)(1U << 31) / 8388607.0f);
	}
	return (samples);
}

size_t
hpsjam_packet::get32Bit1ChSample(float *left) const
{
	const size_t samples = length - 1;

	for (size_t x = 0; x != samples; x++) {
		left[x] = audio_decode(getS32(4 * x), (float)(1U << 31) / 2147483647.0f);
	}
	return (samples);
}

void
hpsjam_packet::put8Bit2ChSample(float *left, float *right, size_t samples)
{
	assert((samples % 2) == 0);

	length = 1 + samples / 2;
	type = HPSJAM_TYPE_AUDIO_8_BIT_2CH;
	sequence[0] = 0;
	sequence[1] = 0;

	for (size_t x = 0; x != samples; x++) {
		putS8(x * 2, audio_encode(left[x], hpsjam_mul_8));
		putS8(x * 2 + 1, audio_encode(right[x], hpsjam_mul_8));
	}
}

void
hpsjam_packet::put16Bit2ChSample(float *left, float *right, size_t samples)
{
	length = 1 + samples;
	type = HPSJAM_TYPE_AUDIO_16_BIT_2CH;
	sequence[0] = 0;
	sequence[1] = 0;

	for (size_t x = 0; x != samples; x++) {
		putS16(x * 4, audio_encode(left[x], hpsjam_mul_16));
		putS16(x * 4 + 2, audio_encode(right[x], hpsjam_mul_16));
	}
}

void
hpsjam_packet::put24Bit2ChSample(float *left, float *right, size_t samples)
{
	length = 1 + (samples * 6 + 3) / 4;
	type = HPSJAM_TYPE_AUDIO_24_BIT_2CH;
	sequence[0] = 0;
	sequence[1] = 0;

	for (size_t x = 0; x != samples; x++) {
		putS24(x * 6, audio_encode(left[x], hpsjam_mul_24));
		putS24(x * 6 + 3, audio_encode(right[x], hpsjam_mul_24));
	}
}

void
hpsjam_packet::put32Bit2ChSample(float *left, float *right, size_t samples)
{
	length = 1 + (samples * 2);
	type = HPSJAM_TYPE_AUDIO_32_BIT_2CH;
	sequence[0] = 0;
	sequence[1] = 0;

	for (size_t x = 0; x != samples; x++) {
		putS32(x * 8, audio_encode(left[x], hpsjam_mul_32));
		putS32(x * 8 + 4, audio_encode(right[x], hpsjam_mul_32));
	}
}

void
hpsjam_packet::put8Bit1ChSample(float *left, size_t samples)
{
	assert((samples % 4) == 0);

	length = 1 + samples / 4;
	type = HPSJAM_TYPE_AUDIO_8_BIT_1CH;
	sequence[0] = 0;
	sequence[1] = 0;

	for (size_t x = 0; x != samples; x++) {
		putS8(x, audio_encode(left[x], hpsjam_mul_8));
	}
}

void
hpsjam_packet::put16Bit1ChSample(float *left, size_t samples)
{
	assert((samples % 2) == 0);

	length = 1 + samples / 2;
	type = HPSJAM_TYPE_AUDIO_16_BIT_1CH;
	sequence[0] = 0;
	sequence[1] = 0;

	for (size_t x = 0; x != samples; x++) {
		putS16(2 * x, audio_encode(left[x], hpsjam_mul_16));
	}
}

void
hpsjam_packet::put24Bit1ChSample(float *left, size_t samples)
{
	length = 1 + (samples * 3 + 3) / 4;
	type = HPSJAM_TYPE_AUDIO_24_BIT_1CH;
	sequence[0] = 0;
	sequence[1] = 0;

	for (size_t x = 0; x != samples; x++) {
		putS24(3 * x, audio_encode(left[x], hpsjam_mul_24));
	}
}

void
hpsjam_packet::put32Bit1ChSample(float *left, size_t samples)
{
	length = 1 + samples;
	type = HPSJAM_TYPE_AUDIO_32_BIT_1CH;
	sequence[0] = 0;
	sequence[1] = 0;

	for (size_t x = 0; x != samples; x++) {
		putS32(4 * x, audio_encode(left[x], hpsjam_mul_32));
	}
}

void
hpsjam_packet::putSilence(size_t samples)
{
	length = 1;
	type = HPSJAM_TYPE_AUDIO_SILENCE;
	sequence[0] = samples & 0xFF;
	sequence[1] = 0;
}

void
hpsjam_packet::putMidiData(const uint8_t *ptr, size_t bytes)
{
	length = 1 + (bytes + 3) / 4;
	type = HPSJAM_TYPE_MIDI_PACKET;
	sequence[0] = (-bytes) & 3;
	sequence[1] = 0;
	/* copy MIDI data in-place */
	memcpy(sequence + 2, ptr, bytes);
	/* zero padding */
	while (bytes & 3)
		sequence[2 + bytes++] = 0;
}

bool
hpsjam_packet::getMidiData(uint8_t *ptr, size_t *pbytes) const
{
	size_t bytes;

	if (length < 2)
		return (false);
	bytes = (length - 1) * 4 - (sequence[0] & 3);
	if (bytes > *pbytes)
		return (false);
	memcpy(ptr, sequence + 2, bytes);
	*pbytes = bytes;
	return (true);
}

size_t
hpsjam_packet::getSilence() const
{
	return (sequence[0]);
}

bool
hpsjam_packet::getFaderValue(uint8_t &mix, uint8_t &index, float *gain, size_t &num) const
{
	if (length >= 2) {
		mix = getS8(0);
		index = getS8(1);
		num = 2 * (length - 2);

		if (getS8(2) & 1) {
			if (num == 0)
				return (false);
			num--;
		}
		for (size_t x = 0; x != num; x++)
			gain[x] = audio_decode(getS16(4 + 2 * x), (float)(1U << 31) / 32767.0f);
		return (true);
	}
	return (false);
};

void
hpsjam_packet::setFaderValue(uint8_t mix, uint8_t index, const float *gain, size_t ngain)
{
	const size_t tot = 2 + ((ngain + 1) / 2);

	assert(tot <= 255);

	length = tot;
	sequence[0] = 0;
	sequence[1] = 0;
	putS8(0, mix);
	putS8(1, index);
	putS8(2, ngain & 1);
	putS8(3, 0);

	for (size_t x = 0; x != ngain; x++)
		putS16(4 + 2 * x, audio_encode(gain[x], hpsjam_mul_16));

	/* zero-pad remainder */
	while (ngain % 2)
		putS16(4 + 2 * ngain++, 0);
};

void
hpsjam_packet::setFaderData(uint8_t mix, uint8_t index, const char *ptr, size_t len)
{
	const size_t tot = 2 + (len + 3) / 4;
	assert(tot <= 255);

	length = tot;
	sequence[0] = 0;
	sequence[1] = 0;
	putS8(0, mix);
	putS8(1, index);
	putS8(2, (-len) & 3);
	putS8(3, 0);
	memcpy(sequence + 6, ptr, len);

	/* zero-pad remainder */
	while (len % 4)
		sequence[6 + len++] = 0;
};

bool
hpsjam_packet::getFaderData(uint8_t &mix, uint8_t &index, const char **pp, size_t &len) const
{
	if (length >= 2) {
		mix = getS8(0);
		index = getS8(1);
		*pp = (const char *)(sequence + 6);
		len = (length - 2) * 4;

		if (getS8(2) & 1) {
			if (len == 0)
				return (false);
			len--;
		}
		if (getS8(2) & 2) {
			if (len < 2)
				return (false);
			len -= 2;
		}
		return (true);
	}
	return (false);
};

void
hpsjam_packet::setRawData(const char *ptr, size_t len, char pad)
{
	const size_t tot = 1 + (len + 3) / 4;
	assert(tot <= 255);

	length = tot;
	sequence[0] = 0;
	sequence[1] = 0;
	memcpy(sequence + 2, ptr, len);

	/* pad remainder */
	while (len % 4)
		sequence[2 + len++] = pad;
};

bool
hpsjam_packet::getRawData(const char **pp, size_t &len) const
{
	if (length >= 1) {
		*pp = (const char *)(sequence + 2);
		len = (length - 1) * 4;
		return (true);
	}
	return (false);
};

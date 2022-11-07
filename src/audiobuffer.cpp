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

#include "audiobuffer.h"
#include "spectralysis.h"

void
hpsjam_audio_buffer :: adjustBuffer()
{
	float buffer[HPSJAM_MAX_SAMPLES];
	float *dst = buffer;
	size_t fwd = HPSJAM_MAX_SAMPLES - consumer;
	size_t num = total;

	/* Copy samples from ring-buffer */
	while (num != 0) {
		if (fwd > num)
			fwd = num;
		memcpy(dst, samples + consumer, sizeof(samples[0]) * fwd);
		dst += fwd;
		num -= fwd;
		consumer += fwd;
		if (consumer == HPSJAM_MAX_SAMPLES) {
			consumer = 0;
			fwd = HPSJAM_MAX_SAMPLES;
		} else {
			assert(num == 0);
			break;
		}
	}

	/* Reset the buffer consumer */
	consumer = 0;

	/* Check for empty buffer */
	if (total == 0) {
		total = 1;
		buffer[0] = last_sample;
		fade_in = fadeSamples;
	}

	int missing = getWaterRef();

	if (missing == 0) {
		memcpy(samples, buffer, sizeof(samples[0]) * total);
	} else if (missing > 0) {
		size_t to = total - missing;
		if (to == 0 || to > HPSJAM_MAX_SAMPLES)
			to = 1;
		for (size_t x = 0; x != to; x++)
			samples[x] = buffer[(total * x) / to];
		total = to;
	} else {
		size_t to = total - missing;
		if (to > HPSJAM_MAX_SAMPLES)
			to = HPSJAM_MAX_SAMPLES;
		if (to > 1 && total > 1) {
			to -= 1;
			total -= 1;

			/* keep last sample the same */
			samples[to] = buffer[total];

			for (size_t x = 0; x < to; ) {
				size_t src = (total * x) / to;
				size_t next;

				/* figure out how many samples to go */
				for (next = x + 1; next != to &&
				     ((total * next) / to) == src; next++)
					;

				float delta = (buffer[src + 1] - buffer[src]) / (ssize_t)(next - x);
				float start = buffer[src];

				/* linear interpolation */
				for (; x < next; x++) {
					samples[x] = start;
					start += delta;
				}
			}
			total = to + 1;
		} else {
			for (size_t x = 0; x != to; x++)
				samples[x] = buffer[(total * x) / to];
			total = to;
		}
	}

	/* Reset the water level after filling samples. */
	low_water = high_water = target_water;
}

/* remove samples from buffer, must be called periodically */
void
hpsjam_audio_buffer :: remSamples(float *dst, size_t num)
{
	size_t fwd;

	doWater(num);

	/* fill missing samples with data from ping pong buffer, if any */
	if (num > total) {
		hpsjam_create_ping_pong_buffer(ping_pong_data, ping_pong_data, ping_pong_offset, fadeSamples);

		float gain = 1.0f;

		/* fill end of buffer using ping pong data */
		for (size_t x = total; x != num; x++) {
			gain -= gain / HPSJAM_SAMPLE_RATE;
			dst[x] = ping_pong_data[(x - total) % fadeSamples] * gain;
		}

		/* update ping pong offset and gain */
		ping_pong_offset = (num - total) % fadeSamples;

		for (size_t x = 0; x != fadeSamples; x++)
			ping_pong_data[x] *= gain;

		/* update last sample */
		last_sample = dst[num - 1];

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
}

/* add samples to buffer */
void
hpsjam_audio_buffer :: addSamples(const float *src, size_t num)
{
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
					const float s = ping_pong_data[(ping_pong_offset + x) % fadeSamples];

					samples[producer + x] = src[x] - f * src[x] + s * f;
					fade_in -= (fade_in != 0);
				}
			} else {
				memcpy(samples + producer, src, sizeof(samples[0]) * fwd);
			}

			/* add all samples to ping pong buffer */
			for (size_t off = (fwd < fadeSamples) ? 0 : (fwd - fadeSamples); off != fwd; off++)
				addPingPongBuffer(samples[producer + off]);

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
}

/* add silence to buffer */
void
hpsjam_audio_buffer :: addSilence(size_t num)
{
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
				addPingPongBuffer(last_sample);
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
}

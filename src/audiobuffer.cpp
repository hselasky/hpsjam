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
hpsjam_audio_buffer :: doAdjustBuffer(int missing)
{
	float buffer[HPSJAM_MAX_SAMPLES];
	float *dst = buffer;
	size_t fwd = HPSJAM_MAX_SAMPLES - consumer;
	size_t num = total;
	size_t to = total - missing;

	/* do nothing, if buffer is empty or nothing is missing */
	if (missing == 0 || total == 0)
		goto done;

	/* check if target buffer size is sane, else return */
	if (to < 2 || to > HPSJAM_MAX_SAMPLES)
		goto done;

	/* move all samples from ring-buffer to temporary buffer */
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

	/* reset the buffer consumer */
	consumer = 0;

	to -= 1;
	total -= 1;

	/* keep last sample the same */
	samples[to] = buffer[total];

	/* linear interpolation */
	for (size_t x = 0; x < to; ) {
		size_t src_start = (total * x) / to;
		size_t src_end;
		size_t dst_next = x + 1;

		/* figure out how many samples to input */
		for (;;) {
			src_end = (total * dst_next) / to;
			if (dst_next == to)
				break;
			if (src_end != src_start)
				break;
			dst_next++;
		}

		float start = 0.0f;

		/* average the samples being compressed */
		for (size_t y = src_start; y != src_end; y++)
			start += buffer[y];

		if (start != 0.0f)
			start /= (ssize_t)(src_end - src_start);

		float delta = (buffer[src_end] - start) / (ssize_t)(dst_next - x);

		for (; x < dst_next; x++) {
			samples[x] = start;
			start += delta;
		}
	}

	/* Set new total buffer size */
	total = to + 1;
done:
	/* Reset the water level after adjusting samples. */
	low_water = high_water = target_water;
}

/* remove samples from buffer, must be called periodically */
void
hpsjam_audio_buffer :: remSamples(float *dst, size_t num)
{
	size_t middle;
	size_t fwd;
	int missing;

	doWater(num);

	/* check if it is time to adjust buffer */
	if (adjust_buffer) {
		missing = getWaterRef();
		middle = (high_water + low_water) / 2;

		/* only adjust when the buffer is above middle full */
		if (total >= middle) {
			doAdjustBuffer(missing);
			adjust_buffer = false;
		}
	}

	/* copy samples from ring-buffer */
	while (num != 0) {
		/* if the buffer is empty, fill it with silence */
		if (total == 0)
			addSilence(fadeSamples);
		/* setup forward size */
		fwd = HPSJAM_MAX_SAMPLES - consumer;
		if (fwd > num)
			fwd = num;
		if (fwd > total)
			fwd = total;
		memcpy(dst, samples + consumer, sizeof(samples[0]) * fwd);
		dst += fwd;
		num -= fwd;
		consumer += fwd;
		total -= fwd;
		if (consumer == HPSJAM_MAX_SAMPLES)
			consumer = 0;
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

			/* add all required samples to ping pong buffer */
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

	/* fill missing samples with data from ping pong buffer, if any */
	hpsjam_create_ping_pong_buffer(ping_pong_data, ping_pong_data,
	    ping_pong_offset, fadeSamples);

	/* copy samples to ring-buffer */
	while (num != 0) {
		if (fwd > num)
			fwd = num;
		if (fwd != 0) {
			float gain = 1.0f;

			/* fill end of buffer using ping pong data */
			for (size_t x = 0; x != fwd; x++) {
				gain -= gain / (HPSJAM_SAMPLE_RATE / 8);
				samples[producer + x] =
				    ping_pong_data[(ping_pong_offset + x) % fadeSamples] * gain;
			}

			/* update ping pong offset and gain */
			ping_pong_offset = (ping_pong_offset + fwd) % fadeSamples;

			for (size_t x = 0; x != fadeSamples; x++)
				ping_pong_data[x] *= gain;

			/* update last sample */
			last_sample = samples[producer + fwd - 1];

			fade_in = fadeSamples;
			num -= fwd;
			total += fwd;
			producer += fwd;
		}
		if (producer == HPSJAM_MAX_SAMPLES) {
			producer = 0;
			fwd = HPSJAM_MAX_SAMPLES;
		}
	}
}

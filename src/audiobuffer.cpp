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

	/* Adjust buffer if outside 4 ms window. */
	int missing_aligned = missing - (missing % (2 * HPSJAM_DEF_SAMPLES));

	if (missing_aligned == 0) {
		memcpy(samples, buffer, sizeof(samples[0]) * total);
	} else if (missing_aligned > 0) {
		size_t to = total - missing;
		if (to == 0 || to > HPSJAM_MAX_SAMPLES)
			to = 1;
		for (size_t x = 0; x != to; x++)
			samples[x] = buffer[(total * x) / to];
		total = to;
	} else {
		size_t to = total - missing;
		if (to == 0 || to > HPSJAM_MAX_SAMPLES)
			to = HPSJAM_MAX_SAMPLES;
		for (size_t x = 0; x != to; x++)
			samples[x] = buffer[(total * x) / to];
		total = to;
	}

	/* Reset the water level after filling samples. */
	low_water = high_water = (HPSJAM_MAX_SAMPLES / 2);
}

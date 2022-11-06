/*-
 * Copyright (c) 2022 Hans Petter Selasky. All rights reserved.
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

#include "hpsjam.h"
#include "spectralysis.h"

void
hpsjam_create_ping_pong_buffer(const float *src, float *dst, size_t last, size_t num)
{
	if (num < 2) {
		if (num == 1)
			dst[0] = src[0];
		return;
	}

	float temp[num];
	float average = 0.0f;
	size_t freq = 0;

	for (size_t i = 0; i != num; i++) {
		temp[i] = src[i];
		average += src[i];
	}

	average /= num;

	for (size_t i = 0; i != num - 1; i++) {
		freq += (temp[i] > average && temp[i + 1] < average);
		freq += (temp[i] < average && temp[i + 1] > average);
	}

	/* first sort all src by value */

	float last_sample = src[(num - 1 + last) % num];

	if (src[(num - 2 + last) % num] > last_sample) {
		/* going down */
		for (size_t i = 0; i != num; i++) {
			for (size_t j = i + 1; j != num; j++) {
				if (temp[i] > temp[j]) {
					float t = temp[j];
					temp[j] = temp[i];
					temp[i] = t;
				}
			}
		}
	} else {
		/* going up */
		for (size_t i = 0; i != num; i++) {
			for (size_t j = i + 1; j != num; j++) {
				if (temp[i] < temp[j]) {
					float t = temp[j];
					temp[j] = temp[i];
					temp[i] = t;
				}
			}
		}
	}

	/* make sure frequency is even */

	freq -= freq % 2;

	/* find the last sample and generate buffer */

	for (size_t i = 0; i != num; i++) {
		if (i != num - 1 && temp[i] != last_sample)
			continue;
		size_t k = 0;
		size_t j = i + freq;

		while (k != num) {
			for (; j < num && k != num; j += freq)
				dst[k++] = temp[j];

			j -= num;

			for (; j < num && k != num; j += freq)
				dst[k++] = temp[num - 1 - j];

			j -= num;
		}
		break;
	}
}

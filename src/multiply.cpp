/*-
 * Copyright (c) 2017-2020 Hans Petter Selasky. All rights reserved.
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

#include "multiply.h"

#ifndef HPSJAM_X3_LOG2_COMBA
#define	HPSJAM_X3_LOG2_COMBA 5
#endif

#if (HPSJAM_X3_LOG2_COMBA < 2)
#error "HPSJAM_X3_LOG2_COMBA must be greater than 1"
#endif

struct hpsjam_x3_input_float {
	float	a;
	float	b;
};

/*
 * <input size> = "stride"
 * <output size> = 2 * "stride"
 */
static void
hpsjam_x3_multiply_sub_float(struct hpsjam_x3_input_float *input, float *ptr_low, float *ptr_high,
    const size_t stride, const uint8_t toggle)
{
	size_t x;
	size_t y;

	if (stride >= (1UL << HPSJAM_X3_LOG2_COMBA)) {
		const size_t strideh = stride >> 1;

		if (toggle) {

			/* inverse step */
			for (x = 0; x != strideh; x++) {
				float a, b, c, d;

				a = ptr_low[x];
				b = ptr_low[x + strideh];
				c = ptr_high[x];
				d = ptr_high[x + strideh];

				ptr_low[x + strideh] = a + b;
				ptr_high[x] = a + b + c + d;
			}

			hpsjam_x3_multiply_sub_float(input, ptr_low, ptr_low + strideh, strideh, 1);

			for (x = 0; x != strideh; x++)
				ptr_low[x + strideh] = -ptr_low[x + strideh];

			hpsjam_x3_multiply_sub_float(input + strideh, ptr_low + strideh, ptr_high + strideh, strideh, 1);

			/* forward step */
			for (x = 0; x != strideh; x++) {
				float a, b, c, d;

				a = ptr_low[x];
				b = ptr_low[x + strideh];
				c = ptr_high[x];
				d = ptr_high[x + strideh];

				ptr_low[x + strideh] = -a - b;
				ptr_high[x] = c + b - d;

				input[x + strideh].a += input[x].a;
				input[x + strideh].b += input[x].b;
			}

			hpsjam_x3_multiply_sub_float(input + strideh, ptr_low + strideh, ptr_high, strideh, 0);
		} else {
			hpsjam_x3_multiply_sub_float(input + strideh, ptr_low + strideh, ptr_high, strideh, 1);

			/* inverse step */
			for (x = 0; x != strideh; x++) {
				float a, b, c, d;

				a = ptr_low[x];
				b = ptr_low[x + strideh];
				c = ptr_high[x];
				d = ptr_high[x + strideh];

				ptr_low[x + strideh] = -a - b;
				ptr_high[x] = a + b + c + d;

				input[x + strideh].a -= input[x].a;
				input[x + strideh].b -= input[x].b;
			}

			hpsjam_x3_multiply_sub_float(input + strideh, ptr_low + strideh, ptr_high + strideh, strideh, 0);

			for (x = 0; x != strideh; x++)
				ptr_low[x + strideh] = -ptr_low[x + strideh];

			hpsjam_x3_multiply_sub_float(input, ptr_low, ptr_low + strideh, strideh, 0);

			/* forward step */
			for (x = 0; x != strideh; x++) {
				float a, b, c, d;

				a = ptr_low[x];
				b = ptr_low[x + strideh];
				c = ptr_high[x];
				d = ptr_high[x + strideh];

				ptr_low[x + strideh] = b - a;
				ptr_high[x] = c - b - d;
			}
		}
	} else {
		for (x = 0; x != stride; x++) {
			float value = input[x].a;

			for (y = 0; y != (stride - x); y++) {
				ptr_low[x + y] += input[y].b * value;
			}

			for (; y != stride; y++) {
				ptr_high[x + y - stride] += input[y].b * value;
			}
		}
	}
}

/*
 * <input size> = "max"
 * <output size> = 2 * "max"
 */
void
hpsjam_x3_multiply_float(const float *va, const float *vb, float *pc, const size_t max)
{
	struct hpsjam_x3_input_float input[max];
	size_t x;

	/* check for non-power of two */
	if (max & (max - 1))
		return;

	/* setup input vector */
	for (x = 0; x != max; x++) {
		input[x].a = va[x];
		input[x].b = vb[x];
	}

	/* do multiplication */
	hpsjam_x3_multiply_sub_float(input, pc, pc + max, max, 1);
}

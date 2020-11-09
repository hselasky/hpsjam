/*-
 * Copyright (c) 2019-2020 Hans Petter Selasky. All rights reserved.
 * Copyright (c) 2019 Google LLC, written by Richard Kralovic <riso@google.com>
 * All rights reserved.
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

#include <stdbool.h>
#include <string.h>
#include <fftw3.h>
#include <math.h>

#include "hpsjam.h"
#include "multiply.h"
#include "equalizer.h"

struct equalizer {
	double rate;
	size_t block_size;
	bool do_normalize;

	/* (block_size * 2) elements, time domain */
	double *fftw_time;

	/* (block_size * 2) elements, half-complex, freq domain */
	double *fftw_freq;

	fftw_plan forward;
	fftw_plan inverse;

	void init(double _rate, size_t _block_size) {
		rate = _rate;
		block_size = _block_size;

		fftw_time = new double [block_size];
		fftw_freq = new double [block_size];

		forward = fftw_plan_r2r_1d(block_size, fftw_time, fftw_freq, FFTW_R2HC, FFTW_MEASURE);
		inverse = fftw_plan_r2r_1d(block_size, fftw_freq, fftw_time, FFTW_HC2R, FFTW_MEASURE);
	};
	void cleanup() {
		fftw_destroy_plan(forward);
		fftw_destroy_plan(inverse);
		delete [] fftw_time;
		delete [] fftw_freq;
	};
	double get_window(double x) {
		return (0.5 + 0.5 * cos(M_PI * x / (block_size / 2))) / block_size;
	};
	bool load_freq_amps(const char *config) {
		double prev_f = 0.0;
		double prev_amp = 1.0;
		double next_f = 0.0;
		double next_amp = 1.0;
		size_t i;

		if (strncasecmp(config, "normalize", 4) == 0) {
			while (*config != 0) {
				if (*config == '\n') {
					config++;
					break;
				}
				config++;
			}
			do_normalize = true;
		} else {
			do_normalize = false;
		}

		for (i = 0; i <= (block_size / 2); ++i) {
			const double f = (i * rate) / block_size;

			while (f >= next_f) {
				prev_f = next_f;
				prev_amp = next_amp;

				if (*config == 0) {
					next_f = rate;
					next_amp = prev_amp;
				} else {
					int len;

					if (sscanf(config, "%lf %lf %n", &next_f, &next_amp, &len) == 2) {
						config += len;
						if (next_f < prev_f)
							return (true);
					} else {
						return (true);
					}
				}
				if (prev_f == 0.0)
					prev_amp = next_amp;
			}
			fftw_freq[i] = ((f - prev_f) / (next_f - prev_f)) * (next_amp - prev_amp) + prev_amp;
		}
		return (false);
	};
	bool load(const char *config) {
		bool retval;
		size_t i;

		memset(fftw_freq, 0, sizeof(fftw_freq[0]) * block_size);

		retval = load_freq_amps(config);
		if (retval)
			return (retval);

		fftw_execute(inverse);

		/* Multiply by symmetric window and shift */
		for (i = 0; i != (block_size / 2); ++i) {
			double weight = get_window(i);

			fftw_time[block_size / 2 + i] = fftw_time[i] * weight;
		}

		for (i = (block_size / 2); i-- > 1; )
			fftw_time[i] = fftw_time[block_size - i];

		fftw_time[0] = 0;

		fftw_execute(forward);

		for (i = 0; i != block_size; i++)
			fftw_freq[i] /= block_size;

		/* Normalize FIR filter, if any */
		if (do_normalize) {
			double sum = 0;

			for (i = 0; i < block_size; ++i)
				sum += fabs(fftw_time[i]);
			if (sum != 0.0) {
				for (i = 0; i < block_size; ++i)
					fftw_time[i] /= sum;
			}
		}
		return (retval);
	};
};

bool
hpsjam_equalizer :: init(const char *pfilter)
{
	/* check if filter starts with filtersize */
	if (strncasecmp(pfilter, "filtersize ", 11) != 0)
		return (true);
	/* get filter size */
	double ms = 1.0;
	if (sscanf(pfilter + 11, "%lfms", &ms) != 1)
		return (true);
	ssize_t osize;
	ssize_t size = osize = (HPSJAM_SAMPLE_RATE * ms) / 1000.0;

	/* limit size of EQ filter */
	if (size < 0)
		size = 0;
	else if (size > 512)
		size = 512;

	/* range check prefilter size */
	if (osize < 0)
		osize = 0;
	else if (osize > HPSJAM_SAMPLE_RATE)
		osize = HPSJAM_SAMPLE_RATE;

	/* skip rest of line */
	while (*pfilter != 0) {
		if (*pfilter == '\n') {
			pfilter++;
			break;
		}
		pfilter++;
	}

	/* make filter size power of two, by rounding down */
	while ((size & -size) != size)
		size -= size & -size;

	/* compute pre-filter size */
	osize -= size;
	if (osize < 0)
		osize = 0;

	/* check if EQ should be disabled */
	if (size == 0) {
		cleanup();
		return (false);
	}

	struct equalizer eq;

	eq.init(HPSJAM_SAMPLE_RATE, size);

	if (eq.load(pfilter)) {
		eq.cleanup();
		return (true);
	}

	if (filter_size != (size_t)size || filter_predelay != (size_t)osize) {
		cleanup();
		filter_data = new float [size];
		filter_in[0] = new float [size];
		filter_in[1] = new float [size];
		filter_out[0] = new float [2 * size + osize];
		filter_out[1] = new float [2 * size + osize];

		memset(filter_out[0], 0, sizeof(float) * (2 * size + osize));
		memset(filter_out[1], 0, sizeof(float) * (2 * size + osize));

		filter_size = size;
		filter_predelay = osize;
	}

	for (ssize_t x = 0; x != size; x++)
		filter_data[x] = eq.fftw_time[x];

	eq.cleanup();
	return (false);
}

void
hpsjam_equalizer :: cleanup()
{
	delete [] filter_data;
	delete [] filter_in[0];
	delete [] filter_in[1];
	delete [] filter_out[0];
	delete [] filter_out[1];

	memset(this, 0, sizeof(*this));
}

void
hpsjam_equalizer :: doit(float *left, float *right, size_t samples)
{
	if (filter_data == 0 || filter_size == 0 || samples == 0)
		return;

	while (1) {
		size_t delta = filter_size - filter_offset;

		if (delta > samples)
			delta = samples;

		for (size_t y = 0; y != delta; y++) {
			filter_in[0][y + filter_offset] = left[y];
			left[y] = filter_out[0][y + filter_offset];

			filter_in[1][y + filter_offset] = right[y];
			right[y] = filter_out[1][y + filter_offset];
		}

		filter_offset += delta;
		samples -= delta;
		left += delta;
		right += delta;

		/* check if there is enough data for a new transform */
		if (filter_offset == filter_size) {
			for (size_t x = 0; x != 2; x++) {
				/* shift down output by filter_size samples */
				for (size_t y = 0; y != filter_size + filter_predelay; y++) {
					filter_out[x][y] = filter_out[x][y + filter_size];
					filter_out[x][y + filter_size] = 0;
				}
				/* perform transform */
				hpsjam_x3_multiply_float(filter_in[x],
				    filter_data, filter_out[x] + filter_predelay, filter_size);
			}
			filter_offset = 0;
		}

		/* check if all samples are consumed */
		if (samples == 0)
			break;
	}
}

void
hpsjam_equalizer :: doit(float *left, size_t samples)
{
	if (filter_data == 0 || filter_size == 0 || samples == 0)
		return;

	while (1) {
		size_t delta = filter_size - filter_offset;

		if (delta > samples)
			delta = samples;

		for (size_t y = 0; y != delta; y++) {
			filter_in[0][y + filter_offset] = left[y];
			left[y] = filter_out[0][y + filter_offset];
		}

		filter_offset += delta;
		samples -= delta;
		left += delta;

		/* check if there is enough data for a new transform */
		if (filter_offset == filter_size) {
			/* shift down output by filter_size samples */
			for (size_t y = 0; y != filter_size + filter_predelay; y++) {
				filter_out[0][y] = filter_out[0][y + filter_size];
				filter_out[0][y + filter_size] = 0;
			}
			/* perform transform */
			hpsjam_x3_multiply_float(filter_in[0],
			    filter_data, filter_out[0] + filter_predelay, filter_size);

			filter_offset = 0;
		}

		/* check if all samples are consumed */
		if (samples == 0)
			break;
	}
}

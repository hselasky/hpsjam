/*-
 * Copyright (c) 2016-2020 Hans Petter Selasky. All rights reserved.
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

#ifndef _HPSJAM_COMPRESSOR_H_
#define	_HPSJAM_COMPRESSOR_H_

static inline bool
hpsjam_float_is_valid(const float x)
{
    const float __r = x * 0.0f;
    return (__r == 0.0f || __r == -0.0f);
}

static inline void
hpsjam_stereo_compressor(const float div, float &pv, float &l, float &r)
{
	/*
	 * Don't max the output range to avoid overflowing sample rate
	 * conversion and equalizer filters in the DSP's output
	 * path. Keep one 10th, 1dB, reserved.
	 */
	constexpr float __limit = 1.0f - (1.0f / 10.0f);

	/* sanity checks */
	if (!hpsjam_float_is_valid(pv))
		pv = 0.0;
	if (!hpsjam_float_is_valid(l))
		l = 0.0;
	if (!hpsjam_float_is_valid(r))
		r = 0.0;
	/* compute maximum */
	if (l < -pv)
		pv = -l;
	else if (l > pv)
		pv = l;
	if (r < -pv)
		pv = -r;
	else if (r > pv)
		pv = r;
	/* compressor */
	if (pv > __limit) {
		l /= pv;
		r /= pv;
		l *= __limit;
		r *= __limit;
		pv -= pv / div;
	}
}

static inline void
hpsjam_mono_compressor(const float div, float &pv, float &l)
{
	/*
	 * Don't max the output range to avoid overflowing sample rate
	 * conversion and equalizer filters in the DSP's output
	 * path. Keep one 10th, 1dB, reserved.
	 */
	constexpr float __limit = 1.0f - (1.0f / 10.0f);

	/* sanity checks */
	if (!hpsjam_float_is_valid(pv))
		pv = 0.0;
	if (!hpsjam_float_is_valid(l))
		l = 0.0;
	/* compute maximum */
	if (l < -pv)
		pv = -l;
	else if (l > pv)
		pv = l;
	/* compressor */
	if (pv > __limit) {
		l /= pv;
		l *= __limit;
		pv -= pv / div;
	}
}

#endif		/* _HPSJAM_COMPRESSOR_H_ */

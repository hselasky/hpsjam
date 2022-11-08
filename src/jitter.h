/*-
 * Copyright (c) 2020-2021 Hans Petter Selasky. All rights reserved.
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

#ifndef _HPSJAM_JITTER_H_
#define	_HPSJAM_JITTER_H_

#include <string.h>

#include "hpsjam.h"
#include "timer.h"

#define	HPSJAM_MAX_JITTER (2U * 16U)	/* ms */

#if (HPSJAM_MAX_JITTER & (HPSJAM_MAX_JITTER - 1))
#error "HPSJAM_MAX_JITTER must be power of two."
#endif

struct hpsjam_jitter {
	float stats[HPSJAM_MAX_JITTER];
	uint64_t packet_recover;
	uint64_t packet_damage;
	uint16_t counter;
	uint16_t jitter_ticks;

	void clear() {
		memset(this, 0, sizeof(*this));
	};
	uint16_t get_jitter_in_ms() {
		return (jitter_ticks);
	};
	void rx_packet() {
		/* assume one packet per tick */
		const uint8_t index = ((uint16_t)(hpsjam_ticks - counter)) % HPSJAM_MAX_JITTER;
		stats[index] += 1.0f;
		counter++;

		if (stats[index] >= HPSJAM_MAX_JITTER) {
			unsigned mask = 0;
			unsigned start;

			for (uint8_t x = 0; x != HPSJAM_MAX_JITTER; x++) {
				stats[x] /= 2.0f;
				mask |= (stats[x] >= 0.5f) << x;
			}

			start = mask;
			for (uint8_t x = 0; x != HPSJAM_MAX_JITTER; x++) {
				if (start > mask)
					start = mask;
				if (mask & 1) {
					mask >>= 1;
					mask |= 1U << (HPSJAM_MAX_JITTER - 1);
				} else {
					mask >>= 1;
				}
			}

			/* recompute jitter_ticks */
			jitter_ticks = 0;
			while (start > 1) {
				jitter_ticks++;
				start /= 2;
			}
		}
	};

	void rx_recover() {
		packet_recover++;
	};

	void rx_damage() {
		packet_damage++;
	};
};

#endif		/* _HPSJAM_JITTER_H_ */

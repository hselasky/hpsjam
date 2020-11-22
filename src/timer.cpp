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
#include <pthread.h>

#if defined(__APPLE__) || defined(__MACOSX)
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <mach/mach_time.h>
#else
#include <sys/time.h>
#endif

#include "hpsjam.h"
#include "timer.h"
#include "peer.h"

uint16_t hpsjam_ticks;
int hpsjam_timer_adjust;

static void
hpsjam_timer_set_priority()
{
	pthread_t pt = pthread_self();
	struct sched_param param;
	int policy;

	pthread_getschedparam(pt, &policy, &param);
	param.sched_priority = sched_get_priority_max(policy);
	pthread_setschedparam(pt, policy, &param);
}

static void *
hpsjam_timer_loop(void *arg)
{
	hpsjam_timer_set_priority();

#if defined(__APPLE__) || defined(__MACOSX)
	struct mach_timebase_info time_base_info;
	uint64_t next;

	mach_timebase_info(&time_base_info);

	const uint64_t delay[3] = {
	    ( 999000ULL * (uint64_t)time_base_info.denom) / (uint64_t)time_base_info.numer,
	    (1000000ULL * (uint64_t)time_base_info.denom) / (uint64_t)time_base_info.numer,
	    (1001000ULL * (uint64_t)time_base_info.denom) / (uint64_t)time_base_info.numer,
	};

	next = mach_absolute_time();
#else
	static const long delay[3] = {
	     999000L,
	    1000000L,
	    1001000L,
	};
	struct timespec next;

	clock_gettime(CLOCK_MONOTONIC, &next);
#endif

	while (1) {
#if defined(__APPLE__) || defined(__MACOSX)
		if (hpsjam_timer_adjust < 0)
			next += delay[0];
		else if (hpsjam_timer_adjust > 0)
			next += delay[2];
		else
			next += delay[1];

		mach_wait_until(next);
#else
		if (hpsjam_timer_adjust < 0)
			next.tv_nsec += delay[0];
		else if (hpsjam_timer_adjust > 0)
			next.tv_nsec += delay[2];
		else
			next.tv_nsec += delay[1];

		if (next.tv_nsec >= 1000000000L) {
			next.tv_sec++;
			next.tv_nsec -= 1000000000L;
		}

		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, 0);
#endif
		if (hpsjam_num_server_peers == 0)
			hpsjam_client_peer->tick();
		else
			hpsjam_server_tick();

		hpsjam_ticks++;
	}
	return (0);
}

Q_DECL_EXPORT void
hpsjam_timer_init()
{
	pthread_t pt;
	int ret;

	ret = pthread_create(&pt, 0, &hpsjam_timer_loop, 0);
	assert(ret == 0);
}

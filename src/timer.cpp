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

#include <assert.h>
#include <pthread.h>

#include <QThread>

#if defined(__APPLE__) || defined(__MACOSX)
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <mach/mach_time.h>
#elif defined(_WIN32)
#include <QElapsedTimer>
static int16_t hpsjam_timer_remainder;
static int16_t hpsjam_timer_next;
static QElapsedTimer hpsjam_timer;
#else
#include <sys/time.h>
#endif

#include "hpsjam.h"
#include "timer.h"
#include "peer.h"

#include <QWaitCondition>

uint16_t hpsjam_ticks;
uint16_t hpsjam_sleep;
int hpsjam_timer_adjust;

static void
hpsjam_timer_set_priority()
{
#ifndef _WIN32
	pthread_t pt = pthread_self();
	struct sched_param param;
	int policy;

	pthread_getschedparam(pt, &policy, &param);
	param.sched_priority = sched_get_priority_max(policy);
	pthread_setschedparam(pt, policy, &param);
#endif
}

static void *
hpsjam_timer_loop(void *)
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
#elif defined(_WIN32)
	hpsjam_timer.start();
	hpsjam_timer.restart();
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
#elif defined(_WIN32)
		hpsjam_timer_remainder += hpsjam_timer_adjust;

		if (hpsjam_timer_remainder <= -1000) {
			hpsjam_timer_remainder += 1000;
		} else if (hpsjam_timer_remainder >= 1000) {
			hpsjam_timer_remainder -= 1000;
			hpsjam_timer_next += 2;
		} else {
			hpsjam_timer_next += 1;
		}
		while (1) {
			int16_t delta = hpsjam_timer_next - hpsjam_timer.elapsed();
			if (delta <= 0)
				break;
			QThread::msleep(1);
		}
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
		if (hpsjam_num_server_peers == 0) {
			hpsjam_client_peer->tick();
		} else {
			if (hpsjam_server_tick()) {
				hpsjam_sleep = 1000;
			} else if (hpsjam_sleep == 0) {
				/* idle for one second */
				QThread::msleep(1000);
#if defined(__APPLE__) || defined(__MACOSX)
				next = mach_absolute_time();
#elif defined(_WIN32)
				hpsjam_timer.restart();
#else
				clock_gettime(CLOCK_MONOTONIC, &next);
#endif
			} else {
				hpsjam_sleep--;
			}
		}

		hpsjam_ticks++;
	}
	return (0);
}

static hpsjam_execute_cb_t *hpsjam_execute_callback;
static uint64_t hpsjam_execute_pending;
static QMutex hpsjam_execute_mtx;
static QWaitCondition hpsjam_execute_wait[2];

static void *
hpsjam_execute_thread(void *arg)
{
	const unsigned shift = (unsigned)((uint8_t *)arg - (uint8_t *)0);
	const uint64_t mask = 1ULL << shift;

	hpsjam_execute_mtx.lock();

	do {
		while ((hpsjam_execute_pending & mask) == 0)
			hpsjam_execute_wait[0].wait(&hpsjam_execute_mtx);
		hpsjam_execute_mtx.unlock();

		hpsjam_execute_callback(shift);

		hpsjam_execute_mtx.lock();
		hpsjam_execute_pending &= ~mask;
		if (mask != 1 && hpsjam_execute_pending == 0)
			hpsjam_execute_wait[1].wakeOne();
	} while (mask != 1);

	hpsjam_execute_mtx.unlock();
	return (0);
}

Q_DECL_EXPORT void
hpsjam_execute(hpsjam_execute_cb_t *cb)
{
	hpsjam_execute_callback = cb;

	hpsjam_execute_mtx.lock();
	if (hpsjam_num_cpu == 64)
		hpsjam_execute_pending = -1ULL;
	else
		hpsjam_execute_pending = (1ULL << hpsjam_num_cpu) - 1ULL;
	hpsjam_execute_wait[0].wakeAll();
	hpsjam_execute_mtx.unlock();

	hpsjam_execute_thread(0);

	hpsjam_execute_mtx.lock();
	while (hpsjam_execute_pending != 0)
		hpsjam_execute_wait[1].wait(&hpsjam_execute_mtx);
	hpsjam_execute_mtx.unlock();
}

Q_DECL_EXPORT void
hpsjam_timer_init()
{
	pthread_t pt;
	int ret;

	ret = pthread_create(&pt, 0, &hpsjam_timer_loop, 0);
	assert(ret == 0);

	/* create additional worker threads, if any */
	for (unsigned x = 1; x != hpsjam_num_cpu; x++) {
		ret = pthread_create(&pt, 0, &hpsjam_execute_thread, (void *)(((uint8_t *)0) + x));
		assert(ret == 0);
	}
}

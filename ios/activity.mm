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

#include <QObject>

#include <stdbool.h>

#include "activity.h"

#include <Foundation/Foundation.h>

static id<NSObject> hpsjam_activity_id = nil;

Q_DECL_EXPORT void
HpsJamBeginActivity()
{
	const NSActivityOptions options =
	    NSActivityBackground | NSActivityIdleSystemSleepDisabled | NSActivityLatencyCritical;

	if (hpsjam_activity_id != nil)
	    return;

	hpsjam_activity_id = [[NSProcessInfo processInfo]
	    beginActivityWithOptions: options
	    reason:@"HPSJAM needs low latency audio and UDP processing and should not be throttled."];
	[hpsjam_activity_id retain];
}

Q_DECL_EXPORT void
HpsJamEndActivity()
{
	if (hpsjam_activity_id == nil)
	    return;

	[[NSProcessInfo processInfo] endActivity: hpsjam_activity_id];
	[hpsjam_activity_id release];
	hpsjam_activity_id = nil;
}

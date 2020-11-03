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

#include "protocol.h"

#include "configdlg.h"

const struct hpsjam_audio_format hpsjam_audio_format[HPSJAM_AUDIO_FORMAT_MAX] = {
	{ HPSJAM_TYPE_AUDIO_8_BIT_1CH, "1CH@8Bit" },
	{ HPSJAM_TYPE_AUDIO_16_BIT_1CH, "1CH@16Bit" },
	{ HPSJAM_TYPE_AUDIO_24_BIT_1CH, "1CH@24Bit" },
	{ HPSJAM_TYPE_AUDIO_32_BIT_1CH, "1CH@32Bit" },
	{ HPSJAM_TYPE_AUDIO_8_BIT_2CH, "2CH@8Bit" },
	{ HPSJAM_TYPE_AUDIO_16_BIT_2CH, "2CH@16Bit" },
	{ HPSJAM_TYPE_AUDIO_24_BIT_2CH, "2CH@24Bit" },
	{ HPSJAM_TYPE_AUDIO_32_BIT_2CH, "2CH@32Bit" },
};

void
HpsJamConfigFormat :: handle_selection()
{
	for (unsigned x = 0; x != HPSJAM_AUDIO_FORMAT_MAX; x++) {
		if (sender() == b + x)
			setIndex(x);
	}
}

void
HpsJamConfig :: handle_config()
{
	configChanged();
}

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

#include <QMutexLocker>
#include <QKeyEvent>

#include "protocol.h"
#include "peer.h"
#include "configdlg.h"

const struct hpsjam_audio_format hpsjam_audio_format[HPSJAM_AUDIO_FORMAT_MAX] = {
	{ HPSJAM_TYPE_AUDIO_SILENCE, "DISABLE" , Qt::Key_0 },
	{ HPSJAM_TYPE_AUDIO_8_BIT_1CH, "1CH@8Bit", Qt::Key_1 },
	{ HPSJAM_TYPE_AUDIO_16_BIT_1CH, "1CH@16Bit", Qt::Key_3 },
	{ HPSJAM_TYPE_AUDIO_24_BIT_1CH, "1CH@24Bit", Qt::Key_5 },
	{ HPSJAM_TYPE_AUDIO_32_BIT_1CH, "1CH@32Bit", Qt::Key_7 },
	{ HPSJAM_TYPE_AUDIO_8_BIT_2CH, "2CH@8Bit", Qt::Key_2 },
	{ HPSJAM_TYPE_AUDIO_16_BIT_2CH, "2CH@16Bit", Qt::Key_4 },
	{ HPSJAM_TYPE_AUDIO_24_BIT_2CH, "2CH@24Bit", Qt::Key_6 },
	{ HPSJAM_TYPE_AUDIO_32_BIT_2CH, "2CH@32Bit", Qt::Key_8 },
};

void
HpsJamDeviceSelection :: handle_toggle_input()
{
#if defined(HAVE_MAC_AUDIO) || defined(HAVE_JACK_AUDIO) || defined(HAVE_ASIO_AUDIO)
	const int input = hpsjam_sound_toggle_input(-1);
#else
	const int input = 0;
#endif
	if (input == -1)
		l_input.setText(tr("Selecting audio input device failed"));
	else if (input == 0)
		l_input.setText(tr("Selected audio input device is system default"));
	else
		l_input.setText(tr("Selected audio input device is %1").arg(input));
}

void
HpsJamDeviceSelection :: handle_toggle_output()
{
#if defined(HAVE_MAC_AUDIO) || defined(HAVE_JACK_AUDIO)
	const int output = hpsjam_sound_toggle_output(-1);
#else
	const int output = 0;
#endif
	if (output == -1)
		l_output.setText(tr("Selecting audio output device failed"));
	else if (output == 0)
		l_output.setText(tr("Selected audio output device is system default"));
	else
		l_output.setText(tr("Selected audio output device is %1").arg(output));
}

void
HpsJamConfigFormat :: handle_selection()
{
	for (unsigned x = 0; x != HPSJAM_AUDIO_FORMAT_MAX; x++) {
		if (sender() == b + x)
			setIndex(x);
	}
}

void
HpsJamConfig :: handle_up_config()
{
	QMutexLocker locker(&hpsjam_client_peer->lock);

	if (hpsjam_client_peer->address.valid())
		hpsjam_client_peer->output_fmt = up_fmt.format;
}

void
HpsJamConfig :: handle_down_config()
{
	QMutexLocker locker(&hpsjam_client_peer->lock);

	if (hpsjam_client_peer->address.valid()) {
		struct hpsjam_packet_entry *pkt = new struct hpsjam_packet_entry;
		pkt->packet.setConfigure(down_fmt.format);
		pkt->packet.type = HPSJAM_TYPE_CONFIGURE_REQUEST;
		pkt->insert_tail(&hpsjam_client_peer->output_pkt.head);
	}
}

void
HpsJamConfig :: keyPressEvent(QKeyEvent *event)
{
	for (unsigned x = 0; x != HPSJAM_AUDIO_FORMAT_MAX; x++) {
		if (event->key() == hpsjam_audio_format[x].key) {
			up_fmt.b[x].animateClick();
			down_fmt.b[x].animateClick();
			return;
		}
	}

	switch (event->key()) {
	case Qt::Key_I:
		audio_dev.b_toggle_input.animateClick();
		break;
	case Qt::Key_O:
		audio_dev.b_toggle_output.animateClick();
		break;
	default:
		break;
	}
}

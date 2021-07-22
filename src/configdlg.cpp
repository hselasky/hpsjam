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

#include <QMutexLocker>
#include <QKeyEvent>

#include "protocol.h"
#include "peer.h"
#include "configdlg.h"
#include "clientdlg.h"
#include "mixerdlg.h"

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

const struct hpsjam_audio_levels hpsjam_audio_levels[HPSJAM_AUDIO_LEVELS_MAX] = {
	{ "OFF", 0.0f },
	{ "LOW", 1.0f / 32.0f },
	{ "MEDIUM", 1.0f / 8.0f },
	{ "HIGH", 1.0f / 2.0f },
	{ "MAX", 1.0f },
};

int
HpsJamDeviceSelection :: handle_toggle_input_device(int value)
{
#if defined(HAVE_MAC_AUDIO) || defined(HAVE_ASIO_AUDIO)
	const int input = hpsjam_sound_toggle_input_device(value);
	refreshStatus();
	return (input);
#else
	return (-1);
#endif
}

int
HpsJamDeviceSelection :: handle_toggle_input_left(int value)
{
#if defined(HAVE_MAC_AUDIO) || defined(HAVE_ASIO_AUDIO)
	const int input = hpsjam_sound_toggle_input_channel(0, value - 1);
	return (input);
#else
	return (-1);
#endif
}

int
HpsJamDeviceSelection :: handle_toggle_input_right(int value)
{
#if defined(HAVE_MAC_AUDIO) || defined(HAVE_ASIO_AUDIO)
	const int input = hpsjam_sound_toggle_input_channel(1, value - 1);
	return (input);
#else
	return (-1);
#endif
}

int
HpsJamDeviceSelection :: handle_toggle_output_device(int value)
{
#if defined(HAVE_MAC_AUDIO)
	const int output = hpsjam_sound_toggle_output_device(value);
	refreshStatus();
	return (output);
#else
	return (-1);
#endif
}

int
HpsJamDeviceSelection :: handle_toggle_output_left(int value)
{
#if defined(HAVE_MAC_AUDIO) || defined(HAVE_ASIO_AUDIO)
	const int output = hpsjam_sound_toggle_output_channel(0, value - 1);
	return (output);
#else
	return (-1);
#endif
}

int
HpsJamDeviceSelection :: handle_toggle_output_right(int value)
{
#if defined(HAVE_MAC_AUDIO) || defined(HAVE_ASIO_AUDIO)
	const int output = hpsjam_sound_toggle_output_channel(1, value - 1);
	return (output);
#else
	return (-1);
#endif
}

int
HpsJamDeviceSelection :: handle_toggle_buffer_samples(int value)
{
#if defined(HAVE_MAC_AUDIO) || defined(HAVE_ASIO_AUDIO)
	if (value == -1) {
		value = hpsjam_sound_toggle_buffer_samples(-1);
		if (value <= 64)
			value = 96;
		else if (value <= 96)
			value = 128;
		else
			value = 64;
		value = hpsjam_sound_toggle_buffer_samples(value);
	} else if (value == 0) {
		value = hpsjam_sound_toggle_buffer_samples(-1);
	} else {
		value = hpsjam_sound_toggle_buffer_samples(value);
	}
	l_buffer_samples.setText(QString("%1 samples").arg(value));
	return (value);
#else
	return (-1);
#endif
}

void
HpsJamDeviceSelection :: refreshStatus()
{
#if defined(HAVE_MAC_AUDIO) || defined(HAVE_ASIO_AUDIO)
	QString status;
	int ch;

	hpsjam_sound_get_input_status(status);
	l_input.setText(status);

	hpsjam_sound_get_output_status(status);
	l_output.setText(status);

	s_input_left.setRange(1, hpsjam_sound_max_input_channel());
	s_input_right.setRange(1, hpsjam_sound_max_input_channel());

	s_output_left.setRange(1, hpsjam_sound_max_output_channel());
	s_output_right.setRange(1, hpsjam_sound_max_output_channel());

	ch = hpsjam_sound_toggle_input_channel(0, -2);
	if (ch < 0)
		ch = 0;
	HPSJAM_NO_SIGNAL(s_input_left,setValue(ch + 1));

	ch = hpsjam_sound_toggle_input_channel(1, -2);
	if (ch < 0)
		ch = 0;
	HPSJAM_NO_SIGNAL(s_input_right,setValue(ch + 1));

	ch = hpsjam_sound_toggle_output_channel(0, -2);
	if (ch < 0)
		ch = 0;
	HPSJAM_NO_SIGNAL(s_output_left,setValue(ch + 1));

	ch = hpsjam_sound_toggle_output_channel(1, -2);
	if (ch < 0)
		ch = 0;
	HPSJAM_NO_SIGNAL(s_output_right,setValue(ch + 1));
#endif
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
HpsJamConfigEffects :: handle_selection()
{
	for (unsigned x = 0; x != HPSJAM_AUDIO_LEVELS_MAX; x++) {
		if (sender() == b + x)
			setIndex(x);
	}
}

void
HpsJamConfigMixer :: handle_selection()
{
	hpsjam_client->w_mixer->setMixerCols(mixer_cols.value());
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
HpsJamConfig :: handle_effects_config()
{
	hpsjam_client->playNewUser();
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
		audio_dev.b_toggle_input_device.animateClick();
		break;
	case Qt::Key_O:
		audio_dev.b_toggle_output_device.animateClick();
		break;
	default:
		break;
	}
}

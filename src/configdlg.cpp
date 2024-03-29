/*-
 * Copyright (c) 2020-2022 Hans Petter Selasky.
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
#include <QListWidgetItem>

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

void
HpsJamDeviceSelection :: handle_rescan_device(bool forced)
{
	int max;
	int in_index;
	int out_index;

	if (forced)
		hpsjam_sound_rescan();

	max = hpsjam_sound_max_devices();
	in_index = hpsjam_sound_set_input_device(-1);
	out_index = hpsjam_sound_set_output_device(-1);

	HPSJAM_NO_SIGNAL(b_input_device,clear());
	HPSJAM_NO_SIGNAL(b_output_device,clear());

	for (int x = 0; x < max; x++) {
		QString name = hpsjam_sound_get_device_name(x);
		QListWidgetItem item(name);

		if (hpsjam_sound_is_input_device(x)) {
			item.setFlags(item.flags() | Qt::ItemIsSelectable | Qt::ItemIsEnabled);
			HPSJAM_NO_SIGNAL(b_input_device,addItem(new QListWidgetItem(item)));
		} else {
			item.setFlags(item.flags() & ~(Qt::ItemIsSelectable | Qt::ItemIsEnabled));
			HPSJAM_NO_SIGNAL(b_input_device,addItem(new QListWidgetItem(item)));
		}
		if (hpsjam_sound_is_output_device(x)) {
			item.setFlags(item.flags() | Qt::ItemIsSelectable | Qt::ItemIsEnabled);
			HPSJAM_NO_SIGNAL(b_output_device,addItem(new QListWidgetItem(item)));
		} else {
			item.setFlags(item.flags() & ~(Qt::ItemIsSelectable | Qt::ItemIsEnabled));
			HPSJAM_NO_SIGNAL(b_output_device,addItem(new QListWidgetItem(item)));
		}
	}

	if (max > 0 && in_index > -1 && in_index < max) {
		HPSJAM_NO_SIGNAL(b_input_device,setCurrentRow(in_index));
		if (forced)
			hpsjam_sound_set_input_device(in_index);
	}

	if (max > 0 && out_index > -1 && out_index < max) {
		HPSJAM_NO_SIGNAL(b_output_device,setCurrentRow(out_index));
		if (forced)
			hpsjam_sound_set_output_device(out_index);
	}
}

void
HpsJamDeviceSelection :: handle_reconfigure_audio()
{
	handle_set_input_device(0);
	handle_set_output_device(0);
}

int
HpsJamDeviceSelection :: handle_set_input_device(int value)
{
	if (value < 0 || hpsjam_sound_is_input_device(value)) {
		const int input = hpsjam_sound_set_input_device(value);
		refreshStatus();
		return (input);
	} else {
		return (-1);
	}
}

int
HpsJamDeviceSelection :: handle_set_input_left(int value)
{
	const int input = hpsjam_sound_set_input_channel(0, value - 1);
	return (input);
}

int
HpsJamDeviceSelection :: handle_set_input_right(int value)
{
	const int input = hpsjam_sound_set_input_channel(1, value - 1);
	return (input);
}

int
HpsJamDeviceSelection :: handle_set_output_device(int value)
{
	if (value < 0 || hpsjam_sound_is_output_device(value)) {
		const int output = hpsjam_sound_set_output_device(value);
		refreshStatus();
		return (output);
	} else {
		return (-1);
	}
}

int
HpsJamDeviceSelection :: handle_set_output_left(int value)
{
	const int output = hpsjam_sound_set_output_channel(0, value - 1);
	return (output);
}

int
HpsJamDeviceSelection :: handle_set_output_right(int value)
{
	const int output = hpsjam_sound_set_output_channel(1, value - 1);
	return (output);
}

int
HpsJamDeviceSelection :: handle_toggle_buffer_samples(int value)
{
	if (value < 0) {
		value = hpsjam_sound_toggle_buffer_samples(-1);
		if (value <= 32)
			value = 64;
		else if (value <= 64)
			value = 96;
		else if (value <= 96)
			value = 128;
		else
			value = 32;
		value = hpsjam_sound_toggle_buffer_samples(value);
	} else if (value == 0) {
		value = hpsjam_sound_toggle_buffer_samples(-1);
	} else {
		value = hpsjam_sound_toggle_buffer_samples(value);
	}
	if (value > -1)
		l_buffer_samples.setText(QString("%1 samples").arg(value));
	else
		l_buffer_samples.setText(QString("System default buffer size"));
	return (value);
}

void
HpsJamDeviceSelection :: handle_set_input_jitter(int value)
{
	int temp[2];

	do {
		QMutexLocker locker(&hpsjam_client_peer->lock);
		temp[0] = hpsjam_client_peer->out_audio[0].setWaterTarget(value);
		temp[1] = hpsjam_client_peer->out_audio[1].setWaterTarget(value);
	} while (0);

	if (temp[0] != value || temp[1] != value)
		HPSJAM_NO_SIGNAL(s_jitter_input,setValue(temp[0]));
}

void
HpsJamDeviceSelection :: handle_set_output_jitter(int value)
{
	int temp[2];

	do {
		QMutexLocker locker(&hpsjam_client_peer->lock);
		temp[0] = hpsjam_client_peer->in_audio[0].setWaterTarget(value);
		temp[1] = hpsjam_client_peer->in_audio[1].setWaterTarget(value);
	} while (0);

	if (temp[0] != value || temp[1] != value)
		HPSJAM_NO_SIGNAL(s_jitter_output,setValue(temp[0]));
}

void
HpsJamDeviceSelection :: refreshStatus()
{
	QString status;
	int ch;

	hpsjam_sound_get_input_status(status);
	l_input.setText(status);

	hpsjam_sound_get_output_status(status);
	l_output.setText(status);

	HPSJAM_NO_SIGNAL(s_input_left,setRange(1, hpsjam_sound_max_input_channel()));
	HPSJAM_NO_SIGNAL(s_input_right,setRange(1, hpsjam_sound_max_input_channel()));

	HPSJAM_NO_SIGNAL(s_output_left,setRange(1, hpsjam_sound_max_output_channel()));
	HPSJAM_NO_SIGNAL(s_output_right,setRange(1, hpsjam_sound_max_output_channel()));

	ch = hpsjam_sound_set_input_channel(0, -1);
	if (ch < 0)
		ch = 0;
	HPSJAM_NO_SIGNAL(s_input_left,setValue(ch + 1));

	ch = hpsjam_sound_set_input_channel(1, -1);
	if (ch < 0)
		ch = 0;
	HPSJAM_NO_SIGNAL(s_input_right,setValue(ch + 1));

	ch = hpsjam_sound_set_output_channel(0, -1);
	if (ch < 0)
		ch = 0;
	HPSJAM_NO_SIGNAL(s_output_left,setValue(ch + 1));

	ch = hpsjam_sound_set_output_channel(1, -1);
	if (ch < 0)
		ch = 0;
	HPSJAM_NO_SIGNAL(s_output_right,setValue(ch + 1));

	handle_rescan_device(false);
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

	if (hpsjam_client_peer->address[0].valid())
		hpsjam_client_peer->output_fmt = up_fmt.format;
}

void
HpsJamConfig :: handle_down_config()
{
	QMutexLocker locker(&hpsjam_client_peer->lock);

	if (hpsjam_client_peer->address[0].valid()) {
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
}

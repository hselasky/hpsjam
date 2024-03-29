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

#include <QPainter>
#include <QKeySequence>
#include <QSettings>

#include "hpsjam.h"

#include "clientdlg.h"
#include "mixerdlg.h"
#include "lyricsdlg.h"
#include "configdlg.h"
#include "chatdlg.h"
#include "connectdlg.h"
#include "statsdlg.h"
#include "helpdlg.h"
#include "recordingdlg.h"
#include "peer.h"

HpsJamClient :: HpsJamClient() : gl(this), b_connect(tr("S&ERVER")),
    b_mixer(tr("&MIXER")), b_lyrics(tr("&LYRICS")), b_chat(tr("CH&AT")),
    b_recording(tr("F&ILE")), b_config(tr("CON&FIG")), b_stats(tr("&STATS")),
    b_help(tr("&HELP"))
{
	setWindowTitle(HPSJAM_WINDOW_TITLE " Client " HPSJAM_VERSION_STRING);
	setWindowIcon(QIcon(QString(HPSJAM_ICON_FILE)));

#if defined(Q_OS_MACX)
	b_connect.setShortcut(QKeySequence(Qt::ALT + Qt::Key_E));
	b_mixer.setShortcut(QKeySequence(Qt::ALT + Qt::Key_M));
	b_lyrics.setShortcut(QKeySequence(Qt::ALT + Qt::Key_L));
	b_chat.setShortcut(QKeySequence(Qt::ALT + Qt::Key_A));
	b_recording.setShortcut(QKeySequence(Qt::ALT + Qt::Key_I));
	b_config.setShortcut(QKeySequence(Qt::ALT + Qt::Key_F));
	b_stats.setShortcut(QKeySequence(Qt::ALT + Qt::Key_S));
	b_help.setShortcut(QKeySequence(Qt::ALT + Qt::Key_H));
#endif

	connect(&b_connect, SIGNAL(released()), this, SLOT(handle_connect()));
	connect(&b_mixer, SIGNAL(released()), this, SLOT(handle_mixer()));
	connect(&b_lyrics, SIGNAL(released()), this, SLOT(handle_lyrics()));
	connect(&b_chat, SIGNAL(released()), this, SLOT(handle_chat()));
	connect(&b_recording, SIGNAL(released()), this, SLOT(handle_recording()));
	connect(&b_config, SIGNAL(released()), this, SLOT(handle_config()));
	connect(&b_stats, SIGNAL(released()), this, SLOT(handle_stats()));
	connect(&b_help, SIGNAL(released()), this, SLOT(handle_help()));

	gl.addWidget(&b_connect, 0,0);
	gl.addWidget(&b_mixer, 0,1);
	gl.addWidget(&b_lyrics, 0,2);
	gl.addWidget(&b_chat, 0,3);
	gl.addWidget(&b_recording, 0,4);
	gl.addWidget(&b_config, 0,5);
	gl.addWidget(&b_stats, 0,6);
	gl.addWidget(&b_help, 0,7);
	gl.addWidget(&w_stack, 1,0,1,9);
	gl.setColumnStretch(8,1);
	gl.setRowStretch(1,1);

	w_connect = new HpsJamConnect();
	w_mixer = new HpsJamMixer();
	w_lyrics = new HpsJamLyrics();
	w_chat = new HpsJamChat();
	w_recording = new HpsJamRecording();
	w_config = new HpsJamConfig();
	w_stats = new HpsJamStats();
	w_help = new HpsJamHelp();

	eq_copy = 0;

	w_stack.addWidget(w_connect);
	w_stack.addWidget(w_mixer);
	w_stack.addWidget(w_lyrics);
	w_stack.addWidget(w_chat);
	w_stack.addWidget(w_recording);
	w_stack.addWidget(w_config);
	w_stack.addWidget(w_stats);
	w_stack.addWidget(w_help);

	connect(&w_config->lyrics_fmt.b_font_select, SIGNAL(released()),
		w_lyrics, SLOT(handle_font_dialog()));

	/* connect client signals */
	connect(hpsjam_client_peer, SIGNAL(receivedFaderLevel(uint8_t,uint8_t,float,float)),
		w_mixer, SLOT(handle_fader_level(uint8_t,uint8_t,float,float)));
	connect(hpsjam_client_peer, SIGNAL(receivedFaderGain(uint8_t,uint8_t,float)),
		w_mixer, SLOT(handle_fader_gain(uint8_t,uint8_t,float)));
	connect(hpsjam_client_peer, SIGNAL(receivedFaderPan(uint8_t,uint8_t,float)),
		w_mixer, SLOT(handle_fader_pan(uint8_t,uint8_t,float)));
	connect(hpsjam_client_peer, SIGNAL(receivedFaderName(uint8_t,uint8_t,QString *)),
		w_mixer, SLOT(handle_fader_name(uint8_t,uint8_t,QString *)));
	connect(hpsjam_client_peer, SIGNAL(receivedFaderIcon(uint8_t,uint8_t,QByteArray *)),
		w_mixer, SLOT(handle_fader_icon(uint8_t,uint8_t,QByteArray *)));
	connect(hpsjam_client_peer, SIGNAL(receivedFaderEQ(uint8_t,uint8_t,QString *)),
		w_mixer, SLOT(handle_fader_eq(uint8_t,uint8_t,QString *)));
	connect(hpsjam_client_peer, SIGNAL(receivedFaderDisconnect(uint8_t,uint8_t)),
		w_mixer, SLOT(handle_fader_disconnect(uint8_t,uint8_t)));
	connect(hpsjam_client_peer, SIGNAL(receivedFaderSelf(uint8_t,uint8_t)),
		w_mixer, SLOT(handle_fader_self(uint8_t,uint8_t)));

	connect(&watchdog, SIGNAL(timeout()), this, SLOT(handle_watchdog()));
	watchdog.start(250);
}

void
HpsJamClient :: handle_connect()
{
	w_stack.setCurrentWidget(w_connect);
	w_connect->setFocus();
}

void
HpsJamClient :: handle_mixer()
{
	w_stack.setCurrentWidget(w_mixer);
	w_mixer->setFocus();
}

void
HpsJamClient :: handle_lyrics()
{
	w_stack.setCurrentWidget(w_lyrics);
	w_lyrics->setFocus();
}

void
HpsJamClient :: handle_chat()
{
	w_stack.setCurrentWidget(w_chat);
	w_chat->chat.line.setFocus();
}

void
HpsJamClient :: handle_recording()
{
	w_stack.setCurrentWidget(w_recording);
	w_recording->setFocus();
}

void
HpsJamClient :: handle_config()
{
	w_stack.setCurrentWidget(w_config);
	w_config->setFocus();
}

void
HpsJamClient :: handle_stats()
{
	w_stack.setCurrentWidget(w_stats);
	w_stats->setFocus();
}

void
HpsJamClient :: handle_help()
{
	w_stack.setCurrentWidget(w_help);
	w_help->setFocus();
}

void
HpsJamClient :: handle_watchdog()
{
	float temp[2];

	if (1) {
		QMutexLocker locker(&hpsjam_client_peer->lock);
		temp[0] = hpsjam_client_peer->out_level[0].getLevel();
		temp[1] = hpsjam_client_peer->out_level[1].getLevel();

		hpsjam_client_peer->bits = w_mixer->self_strip.getBits();
		const float mg[2] = {
			level_decode(w_mixer->self_strip.w_slider.value),
			level_decode(1.0f - w_mixer->self_strip.w_slider.value),
		};
		hpsjam_client_peer->mon_gain[0] = mg[0] / (mg[0] + mg[1]);
		hpsjam_client_peer->mon_gain[1] = mg[1] / (mg[0] + mg[1]);

		hpsjam_client_peer->mon_pan = w_mixer->self_strip.w_slider.pan;
	}

	w_mixer->self_strip.w_slider.setLevel(level_encode(temp[0]), level_encode(temp[1]));
}

void
HpsJamClient :: closeEvent(QCloseEvent *event)
{
	handle_connect();
	w_connect->handle_disconnect();

	/* save settings before stopping audio device */
	saveSettings();

	hpsjam_sound_uninit();

	QCoreApplication::exit(0);
}

static QString
HpsJamGetProfile()
{
	if (hpsjam_profile_index == 0)
		return (QString("HpsJam"));
	else
		return (QString("HpsJam%1").arg(hpsjam_profile_index));
}

void
HpsJamClient :: saveSettings()
{
	QSettings settings(HpsJamGetProfile());

	settings.beginGroup("connect");
	settings.setValue("username", w_connect->name.edit.text());
	settings.setValue("icon", w_connect->icon.selection);
	settings.setValue("password", w_connect->password.edit.text());
	settings.setValue("server", w_connect->server.edit.text());
	settings.setValue("list", w_connect->server.list.toPlainText());
	settings.endGroup();

	settings.beginGroup("config");
	settings.setValue("effects_level", w_config->effects.selection);
	settings.setValue("uplink_format", w_config->up_fmt.selection);
	settings.setValue("downlink_format", w_config->down_fmt.selection);
	settings.setValue("mixer_cols", w_config->mixer.mixer_cols.value());
	settings.setValue("input_device", w_config->audio_dev.handle_set_input_device());
	settings.setValue("output_device", w_config->audio_dev.handle_set_output_device());
	settings.setValue("input_left", w_config->audio_dev.handle_set_input_left());
	settings.setValue("output_left", w_config->audio_dev.handle_set_output_left());
	settings.setValue("input_right", w_config->audio_dev.handle_set_input_right());
	settings.setValue("output_right", w_config->audio_dev.handle_set_output_right());
	settings.setValue("buffer_samples", w_config->audio_dev.handle_toggle_buffer_samples(0));
	settings.setValue("output_jitter", w_config->audio_dev.s_jitter_output.value());
	settings.setValue("input_jitter", w_config->audio_dev.s_jitter_input.value());
	settings.endGroup();
}

void
HpsJamClient :: loadSettings()
{
	QSettings settings(HpsJamGetProfile());

	w_connect->name.edit.setText(settings.value("connect/username").toString());
	w_connect->icon.setSelection(settings.value("connect/icon", QString("0")).toInt());
	w_connect->password.edit.setText(settings.value("connect/password", QString()).toString());
	w_connect->server.edit.setText(settings.value("connect/server", QString("127.0.0.1:" HPSJAM_DEFAULT_PORT_STR)).toString());
	HPSJAM_NO_SIGNAL(w_connect->server.list,setPlainText(settings.value("connect/list", QString()).toString()));
	w_connect->server.list.updateSelection(w_connect->server.edit.text());

	HPSJAM_NO_SIGNAL(w_config->effects,setIndex(settings.value("config/effects_level", QString("0")).toInt()));
	w_config->up_fmt.setIndex(settings.value("config/uplink_format", QString("1")).toInt());
	w_config->down_fmt.setIndex(settings.value("config/downlink_format", QString("1")).toInt());
#if (HPSJAM_PEERS_MAX + 1 != 257)
#error "Please update the defaults below"
#endif
	w_config->mixer.mixer_cols.setValue(settings.value("config/mixer_cols", QString("257")).toInt());

	input_device = settings.value("config/input_device", QString("-1")).toInt();
	output_device = settings.value("config/output_device", QString("-1")).toInt();
	input_left = settings.value("config/input_left", QString("-1")).toInt();
	output_left = settings.value("config/output_left", QString("-1")).toInt();
	input_right = settings.value("config/input_right", QString("-1")).toInt();
	output_right = settings.value("config/output_right", QString("-1")).toInt();
	buffer_samples = settings.value("config/buffer_samples", QString("96")).toInt();

	w_config->audio_dev.s_jitter_output.setValue(settings.value("config/output_jitter", QString("8")).toInt());
	w_config->audio_dev.s_jitter_input.setValue(settings.value("config/input_jitter", QString("8")).toInt());
}

void
HpsJamClient :: playNewUser()
{
	float gain = hpsjam_audio_levels[w_config->effects.selection].gain;

	QMutexLocker locker(&hpsjam_client_peer->lock);
	hpsjam_client_peer->audio_effects.playNewUser(gain);
}

void
HpsJamClient :: playNewMessage()
{
	float gain = hpsjam_audio_levels[w_config->effects.selection].gain;

	QMutexLocker locker(&hpsjam_client_peer->lock);
	hpsjam_client_peer->audio_effects.playNewMessage(gain);
}

bool
HpsJamClient :: pullPlayback(float *pl, float *pr, size_t num)
{
	bool retval = w_recording->playback.b_start.isFlat();
	if (retval)
		w_recording->playbuf.pull_add_samples(pl, pr, num);
	return (retval);
}

void
HpsJamClient :: pushRecord(float *pl, float *pr, size_t num)
{
	if (w_recording->recording.b_start.isFlat())
		w_recording->recbuf.push_samples(pl, pr, num);
}

void
HpsJamClientButton :: handle_timeout()
{
	if (flashing) {
		flashstate = !flashstate;
		update();
	}
}

void
HpsJamClientButton :: handle_released()
{
	flashing = false;
	watchdog.stop();
	update();
}

void
HpsJamClientButton :: paintEvent(QPaintEvent *event)
{
	_flash = flashing && flashstate;

	HpsJamPushButton::paintEvent(event);
}

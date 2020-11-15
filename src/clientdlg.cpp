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

#include <QPainter>

#include "hpsjam.h"

#include "clientdlg.h"
#include "mixerdlg.h"
#include "lyricsdlg.h"
#include "configdlg.h"
#include "chatdlg.h"
#include "connectdlg.h"
#include "statsdlg.h"
#include "help.h"
#include "peer.h"

HpsJamClient :: HpsJamClient() : gl(this), b_connect(tr("CONN&ECT")),
    b_mixer(tr("&MIXER")), b_lyrics(tr("&LYRICS")), b_chat(tr("CH&AT")),
    b_config(tr("CON&FIG")), b_stats(tr("&STATS")), b_help(tr("&HELP"))
{
	setWindowTitle(HPSJAM_WINDOW_TITLE " Client");
	setWindowIcon(QIcon(QString(HPSJAM_ICON_FILE)));

	connect(&b_connect, SIGNAL(released()), this, SLOT(handle_connect()));
	connect(&b_mixer, SIGNAL(released()), this, SLOT(handle_mixer()));
	connect(&b_lyrics, SIGNAL(released()), this, SLOT(handle_lyrics()));
	connect(&b_chat, SIGNAL(released()), this, SLOT(handle_chat()));
	connect(&b_config, SIGNAL(released()), this, SLOT(handle_config()));
	connect(&b_stats, SIGNAL(released()), this, SLOT(handle_stats()));
	connect(&b_help, SIGNAL(released()), this, SLOT(handle_help()));

	gl.addWidget(&b_connect, 0,0);
	gl.addWidget(&b_mixer, 0,1);
	gl.addWidget(&b_lyrics, 0,2);
	gl.addWidget(&b_chat, 0,3);
	gl.addWidget(&b_config, 0,4);
	gl.addWidget(&b_stats, 0,5);
	gl.addWidget(&b_help, 0,6);
	gl.addWidget(&w_stack, 1,0,1,8);
	gl.setColumnStretch(7,1);
	gl.setRowStretch(1,1);

	w_connect = new HpsJamConnect();
	w_mixer = new HpsJamMixer();
	w_lyrics = new HpsJamLyrics();
	w_chat = new HpsJamChat();
	w_config = new HpsJamConfig();
	w_stats = new HpsJamStats();
	w_help = new HpsJamHelp();

	eq_copy = 0;

	w_stack.addWidget(w_connect);
	w_stack.addWidget(w_mixer);
	w_stack.addWidget(w_lyrics);
	w_stack.addWidget(w_chat);
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

	QCoreApplication::exit(0);
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
	static const QColor fg(255,255,255,192);

	QPushButton::paintEvent(event);

	if (flashing && flashstate) {
		QPainter paint(this);
		paint.fillRect(QRect(0,0,width(),height()), fg);
	}
}

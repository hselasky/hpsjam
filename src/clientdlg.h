/*-
 * Copyright (c) 2020-2022 Hans Petter Selasky. All rights reserved.
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

#ifndef _HPSJAM_CLIENTDLG_H_
#define	_HPSJAM_CLIENTDLG_H_

#include <QCoreApplication>

#include <QWidget>
#include <QStackedWidget>
#include <QGridLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>

#include "texture.h"

class HpsJamConnect;
class HpsJamMixer;
class HpsJamLyrics;
class HpsJamChat;
class HpsJamRecording;
class HpsJamConfig;
class HpsJamStats;
class HpsJamEqualizer;
class HpsJamHelp;

class HpsJamMessageBox : QTimer
{
	Q_OBJECT

	QWidget *_parent;
	QString _title;
	QString _text;
public:
	HpsJamMessageBox(QWidget *parent, const QString &title, const QString &text) :
		_parent(parent), _title(title), _text(text)
	{
		connect(this, SIGNAL(timeout()), this, SLOT(handle_timer()));
		setSingleShot(true);
		start();
	}
public slots:
	void handle_timer()
	{
		QMessageBox::information(_parent, _title, _text);
		deleteLater();
	}
};

class HpsJamClientButton : public HpsJamPushButton {
	Q_OBJECT
public:
	HpsJamClientButton(const QString &str) : HpsJamPushButton(str) {
		flashing = false;
		flashstate = false;
		connect(&watchdog, SIGNAL(timeout()), this, SLOT(handle_timeout()));
		connect(this, SIGNAL(released()), this, SLOT(handle_released()));
	};
	bool flashing;
	bool flashstate;
	QTimer watchdog;

	void paintEvent(QPaintEvent *);
	void setFlashing() {
		watchdog.start(1000);
		flashing = true;
		flashstate = false;
	};
public slots:
	void handle_timeout();
	void handle_released();
};

class HpsJamClient : public HpsJamTWidget {
	Q_OBJECT
public:
	HpsJamClient();
	QGridLayout gl;
	QStackedWidget w_stack;
	HpsJamClientButton b_connect;
	HpsJamClientButton b_mixer;
	HpsJamClientButton b_lyrics;
	HpsJamClientButton b_chat;
	HpsJamClientButton b_recording;
	HpsJamClientButton b_config;
	HpsJamClientButton b_stats;
	HpsJamClientButton b_help;
	QTimer watchdog;

	HpsJamConnect *w_connect;
	HpsJamMixer *w_mixer;
	HpsJamLyrics *w_lyrics;
	HpsJamChat *w_chat;
	HpsJamRecording *w_recording;
	HpsJamConfig *w_config;
	HpsJamStats *w_stats;
	HpsJamEqualizer *eq_copy;
	HpsJamHelp *w_help;

	/* settings */
	int input_device;
	int output_device;
	int input_left;
	int output_left;
	int input_right;
	int output_right;
	int buffer_samples;

	void loadSettings();
	void saveSettings();

	void playNewUser();
	void playNewMessage();

	void closeEvent(QCloseEvent *event);

	bool pullPlayback(float *, float *, size_t);
	void pushRecord(float *, float *, size_t);

public slots:
	void handle_connect();
	void handle_mixer();
	void handle_lyrics();
	void handle_chat();
	void handle_recording();
	void handle_config();
	void handle_stats();
	void handle_help();
	void handle_watchdog();
};

#endif		/* _HPSJAM_CLIENTDLG_H_ */

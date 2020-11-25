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

#ifndef _HPSJAM_CONFIGDLG_H_
#define	_HPSJAM_CONFIGDLG_H_

#include "hpsjam.h"

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QGridLayout>
#include <QGroupBox>

struct hpsjam_audio_format {
	uint8_t format;
	const char *descr;
	int key;
};

extern const struct hpsjam_audio_format hpsjam_audio_format[HPSJAM_AUDIO_FORMAT_MAX];

class HpsJamDeviceSelection : public QGroupBox {
	Q_OBJECT;
public:
	HpsJamDeviceSelection() : gl(this),
	    b_toggle_input(tr("Toggle input device")),
	    b_toggle_output(tr("Toggle output device")) {
#if defined(HAVE_ASIO_AUDIO)
		b_toggle_output.setEnabled(false);
#endif
		setTitle("Audio device configuration");
		gl.addWidget(&b_toggle_input, 0,0);
		gl.addWidget(&l_input, 0,1);
		gl.addWidget(&b_toggle_output, 1,0);
		gl.addWidget(&l_output, 1,1);
		gl.setColumnStretch(1,1);

		connect(&b_toggle_input, SIGNAL(released()), this, SLOT(handle_toggle_input()));
		connect(&b_toggle_output, SIGNAL(released()), this, SLOT(handle_toggle_output()));
	};
	QGridLayout gl;
	QPushButton b_toggle_input;
	QPushButton b_toggle_output;
	QLabel l_input;
	QLabel l_output;

public slots:
	void handle_toggle_input();
	void handle_toggle_output();
};

class HpsJamConfigFormat : public QGroupBox {
	Q_OBJECT;
public:
	HpsJamConfigFormat() : gl(this) {
		for (unsigned x = 0; x != HPSJAM_AUDIO_FORMAT_MAX; x++) {
			b[x].setFlat(x != 1);
			b[x].setText(QString(hpsjam_audio_format[x].descr));
			connect(&b[x], SIGNAL(released()), this, SLOT(handle_selection()));
			if (x == 0)
				gl.addWidget(b + x, 0, 0);
			else
				gl.addWidget(b + x, 1 + ((x - 1) / 4), (x - 1) % 4);
		}
		format = hpsjam_audio_format[1].format;
	};
	uint8_t format;
	QPushButton b[HPSJAM_AUDIO_FORMAT_MAX];
	QGridLayout gl;

	void setIndex(unsigned index) {
		for (unsigned x = 0; x != HPSJAM_AUDIO_FORMAT_MAX; x++) {
			b[x].setFlat(x != index);
			if (x == index) {
				if (hpsjam_audio_format[x].format != format) {
					format = hpsjam_audio_format[x].format;
					valueChanged();
				}
			}
		}
	};
public slots:
	void handle_selection();
signals:
	void valueChanged();
};

class HpsJamLyricsFormat : public QGroupBox {
public:
	HpsJamLyricsFormat() : gl(this), b_font_select(tr("Select font")) {
		gl.addWidget(&b_font_select, 0,0);
		setTitle(tr("Lyrics format"));
	};
	QGridLayout gl;
	QPushButton b_font_select;
};

class HpsJamConfig : public QWidget {
	Q_OBJECT;
public:
	HpsJamConfig() : gl(this) {
		up_fmt.setTitle(tr("Uplink audio format"));
		down_fmt.setTitle(tr("Downlink audio format"));

		gl.addWidget(&up_fmt, 0,0);
		gl.addWidget(&down_fmt, 1,0);
		gl.addWidget(&audio_dev, 2,0);
		gl.addWidget(&lyrics_fmt, 3,0);
		gl.setRowStretch(4,1);

		connect(&up_fmt, SIGNAL(valueChanged()), this, SLOT(handle_up_config()));
		connect(&down_fmt, SIGNAL(valueChanged()), this, SLOT(handle_down_config()));
	};
	void keyPressEvent(QKeyEvent *);
	QGridLayout gl;
	HpsJamConfigFormat up_fmt;
	HpsJamConfigFormat down_fmt;
	HpsJamDeviceSelection audio_dev;
	HpsJamLyricsFormat lyrics_fmt;
public slots:
	void handle_up_config();
	void handle_down_config();
};

#endif		/* _HPSJAM_CONFIGDLG_H_ */

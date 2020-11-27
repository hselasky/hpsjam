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
#if defined(HAVE_ASIO_AUDIO)
	    b_toggle_input_device(tr("Toggle device")),
#else
	    b_toggle_input_device(tr("Toggle input device")),
#endif
	    b_toggle_input_left(tr("L-IN")),
	    b_toggle_input_right(tr("R-IN")),
	    b_toggle_output_device(tr("Toggle output device")),
	    b_toggle_output_left(tr("L-OUT")),
	    b_toggle_output_right(tr("R-OUT")) {
		setTitle("Audio device configuration");
#if defined(HAVE_MAC_AUDIO)
		gl.addWidget(&b_toggle_input_device, 0,0);
		gl.addWidget(&b_toggle_input_left, 0,1);
		gl.addWidget(&b_toggle_input_right, 0,2);
		gl.addWidget(&l_input, 0,3);

		gl.addWidget(&b_toggle_output_device, 1,0);
		gl.addWidget(&b_toggle_output_left, 1,1);
		gl.addWidget(&b_toggle_output_right, 1,2);
		gl.addWidget(&l_output, 1,3);
#endif

#if defined(HAVE_ASIO_AUDIO)
		gl.addWidget(&b_toggle_input_device, 0,0);
		gl.addWidget(&b_toggle_input_left, 0,1);
		gl.addWidget(&b_toggle_input_right, 0,2);
		gl.addWidget(&l_input, 0,3);

		gl.addWidget(&b_toggle_output_left, 1,1);
		gl.addWidget(&b_toggle_output_right, 1,2);
#endif
		gl.setColumnStretch(3,1);

		connect(&b_toggle_input_device, SIGNAL(released()), this, SLOT(handle_toggle_input_device()));
		connect(&b_toggle_output_device, SIGNAL(released()), this, SLOT(handle_toggle_output_device()));

		connect(&b_toggle_input_left, SIGNAL(released()), this, SLOT(handle_toggle_input_left()));
		connect(&b_toggle_output_left, SIGNAL(released()), this, SLOT(handle_toggle_output_left()));

		connect(&b_toggle_input_right, SIGNAL(released()), this, SLOT(handle_toggle_input_right()));
		connect(&b_toggle_output_right, SIGNAL(released()), this, SLOT(handle_toggle_output_right()));
	};
	QGridLayout gl;
	QPushButton b_toggle_input_device;
	QPushButton b_toggle_input_left;
	QPushButton b_toggle_input_right;
	QPushButton b_toggle_output_device;
	QPushButton b_toggle_output_left;
	QPushButton b_toggle_output_right;
	QLabel l_input;
	QLabel l_output;

public slots:
	int handle_toggle_input_device(int = -1);
	int handle_toggle_output_device(int = -1);
	int handle_toggle_input_left(int = -1);
	int handle_toggle_output_left(int = -1);
	int handle_toggle_input_right(int = -1);
	int handle_toggle_output_right(int = -1);
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
		selection = 1;
	};
	uint8_t format;
	uint8_t selection;
	QPushButton b[HPSJAM_AUDIO_FORMAT_MAX];
	QGridLayout gl;

	void setIndex(unsigned index) {
		for (unsigned x = 0; x != HPSJAM_AUDIO_FORMAT_MAX; x++) {
			b[x].setFlat(x != index);
			if (x != index || hpsjam_audio_format[x].format == format)
				continue;
			format = hpsjam_audio_format[x].format;
			selection = index;
			valueChanged();
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

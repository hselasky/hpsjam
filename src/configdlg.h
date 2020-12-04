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
#include <QSpinBox>

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
	    b_toggle_output_device(tr("Toggle output device")) {
		setTitle("Audio device configuration");

		s_input_left.setAccessibleDescription("Set left input channel index");
		s_input_left.setPrefix(QString("L-IN "));

		s_input_right.setAccessibleDescription("Set right input channel index");
		s_input_right.setPrefix(QString("R-IN "));

		s_output_left.setAccessibleDescription("Set left output channel index");
		s_output_left.setPrefix(QString("L-OUT "));

		s_output_right.setAccessibleDescription("Set right output channel index");
		s_output_right.setPrefix(QString("R-OUT "));

#if defined(HAVE_MAC_AUDIO)
		gl.addWidget(&b_toggle_input_device, 0,0);
		gl.addWidget(&s_input_left, 0,1);
		gl.addWidget(&s_input_right, 0,2);
		gl.addWidget(&l_input, 0,3);

		gl.addWidget(&b_toggle_output_device, 1,0);
		gl.addWidget(&s_output_left, 1,1);
		gl.addWidget(&s_output_right, 1,2);
		gl.addWidget(&l_output, 1,3);
#endif

#if defined(HAVE_ASIO_AUDIO)
		gl.addWidget(&b_toggle_input_device, 0,0);
		gl.addWidget(&s_input_left, 0,1);
		gl.addWidget(&s_input_right, 0,2);
		gl.addWidget(&l_input, 0,3,2,1);

		gl.addWidget(&s_output_left, 1,1);
		gl.addWidget(&s_output_right, 1,2);
#endif
		gl.setColumnStretch(3,1);

		connect(&b_toggle_input_device, SIGNAL(released()), this, SLOT(handle_toggle_input_device()));
		connect(&b_toggle_output_device, SIGNAL(released()), this, SLOT(handle_toggle_output_device()));

		connect(&s_input_left, SIGNAL(valueChanged(int)), this, SLOT(handle_toggle_input_left(int)));
		connect(&s_output_left, SIGNAL(valueChanged(int)), this, SLOT(handle_toggle_output_left(int)));

		connect(&s_input_right, SIGNAL(valueChanged(int)), this, SLOT(handle_toggle_input_right(int)));
		connect(&s_output_right, SIGNAL(valueChanged(int)), this, SLOT(handle_toggle_output_right(int)));
	};
	QGridLayout gl;
	QPushButton b_toggle_input_device;
	QSpinBox s_input_left;
	QSpinBox s_input_right;
	QPushButton b_toggle_output_device;
	QSpinBox s_output_left;
	QSpinBox s_output_right;
	QLabel l_input;
	QLabel l_output;

	void refreshStatus();

public slots:
	int handle_toggle_input_device(int = -1);
	int handle_toggle_output_device(int = -1);
	int handle_toggle_input_left(int);
	int handle_toggle_output_left(int);
	int handle_toggle_input_right(int);
	int handle_toggle_output_right(int);
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
	QString description;

	void titleRegen() {
		setTitle(description + QString(" :: %1").arg(hpsjam_audio_format[selection].descr));
	};

	void setIndex(unsigned index) {
		for (unsigned x = 0; x != HPSJAM_AUDIO_FORMAT_MAX; x++) {
			b[x].setFlat(x != index);
			if (x != index || hpsjam_audio_format[x].format == format)
				continue;
			format = hpsjam_audio_format[x].format;
			selection = index;
			titleRegen();
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
		up_fmt.description = tr("Uplink audio format");
		up_fmt.titleRegen();

		down_fmt.description = tr("Downlink audio format");
		down_fmt.titleRegen();

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

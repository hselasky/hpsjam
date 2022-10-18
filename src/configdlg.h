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

#ifndef _HPSJAM_CONFIGDLG_H_
#define	_HPSJAM_CONFIGDLG_H_

#include "texture.h"

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QGridLayout>
#include <QSpinBox>
#include <QListWidget>

struct hpsjam_audio_format {
	uint8_t format;
	const char *descr;
	int key;
};

extern const struct hpsjam_audio_format hpsjam_audio_format[HPSJAM_AUDIO_FORMAT_MAX];

class HpsJamDeviceSelection : public HpsJamGroupBox {
	Q_OBJECT
public:
	HpsJamDeviceSelection() :
	    b_input_device(),
	    b_output_device(),
	    l_jitter_input(tr("Local audio input jitter buffer")),
	    l_jitter_output(tr("Local audio output jitter buffer")),
	    b_toggle_buffer_samples(tr("Toggle buffer size")),
	    b_rescan_device("Rescan devices") {
		setTitle("Audio device configuration");

		s_input_left.setAccessibleDescription("Set left input channel index");
		s_input_left.setPrefix(QString("L-IN "));

		s_input_right.setAccessibleDescription("Set right input channel index");
		s_input_right.setPrefix(QString("R-IN "));

		s_output_left.setAccessibleDescription("Set left output channel index");
		s_output_left.setPrefix(QString("L-OUT "));

		s_output_right.setAccessibleDescription("Set right output channel index");
		s_output_right.setPrefix(QString("R-OUT "));

		s_jitter_input.setAccessibleDescription("Set local audio input jitter buffer in milliseconds");
		s_jitter_input.setSuffix(QString(" ms"));
		s_jitter_input.setRange(0,99);

		s_jitter_output.setAccessibleDescription("Set local audio output jitter buffer in milliseconds");
		s_jitter_output.setSuffix(QString(" ms"));
		s_jitter_output.setRange(0,99);

		gl.addWidget(&s_jitter_input, 0,1);
		gl.addWidget(&l_jitter_input, 0,2,1,2);
		gl.addWidget(&b_input_device, 0,0,2,1);
		gl.addWidget(&s_input_left, 1,1);
		gl.addWidget(&s_input_right, 1,2);
		gl.addWidget(&l_input, 1,3);

		gl.addWidget(&s_jitter_output, 2,1);
		gl.addWidget(&l_jitter_output, 2,2,1,2);
#if !defined(HAVE_ASIO_AUDIO)
		gl.addWidget(&b_output_device, 2,0,2,1);
#endif
		gl.addWidget(&s_output_left, 3,1);
		gl.addWidget(&s_output_right, 3,2);

#if !defined(HAVE_ASIO_AUDIO)
		gl.addWidget(&l_output, 3,3);
#endif

		gl.addWidget(&b_toggle_buffer_samples, 4,1);
		gl.addWidget(&l_buffer_samples, 4,2,1,2);
		gl.addWidget(&b_rescan_device, 4,0,1,1);

		connect(&b_input_device, SIGNAL(currentRowChanged(int)), this, SLOT(handle_set_input_device(int)));
		connect(&b_output_device, SIGNAL(currentRowChanged(int)), this, SLOT(handle_set_output_device(int)));

		connect(&s_input_left, SIGNAL(valueChanged(int)), this, SLOT(handle_set_input_left(int)));
		connect(&s_output_left, SIGNAL(valueChanged(int)), this, SLOT(handle_set_output_left(int)));

		connect(&s_jitter_input, SIGNAL(valueChanged(int)), this, SLOT(handle_set_input_jitter(int)));
		connect(&s_jitter_output, SIGNAL(valueChanged(int)), this, SLOT(handle_set_output_jitter(int)));

		connect(&s_input_right, SIGNAL(valueChanged(int)), this, SLOT(handle_set_input_right(int)));
		connect(&s_output_right, SIGNAL(valueChanged(int)), this, SLOT(handle_set_output_right(int)));

		connect(&b_toggle_buffer_samples, SIGNAL(released()), this, SLOT(handle_toggle_buffer_samples()));
		connect(&b_rescan_device, SIGNAL(released()), this, SLOT(handle_rescan_device()));

		connect(this, SIGNAL(reconfigure_audio()), this, SLOT(handle_reconfigure_audio()));

		handle_toggle_buffer_samples(0);

		setCollapsed(true);
	};
	QListWidget b_input_device;
	QSpinBox s_input_left;
	QSpinBox s_input_right;
	QSpinBox s_jitter_input;
	QSpinBox s_jitter_output;
	QListWidget b_output_device;
	QSpinBox s_output_left;
	QSpinBox s_output_right;
	QLabel l_input;
	QLabel l_output;
	QLabel l_jitter_input;
	QLabel l_jitter_output;
	HpsJamPushButton b_toggle_buffer_samples;
	HpsJamPushButton b_rescan_device;
	QLabel l_buffer_samples;

	void refreshStatus();
signals:
	void reconfigure_audio();

public slots:
	void handle_reconfigure_audio();
	void handle_rescan_device(bool = true);
	int handle_set_input_device(int = -1);
	int handle_set_output_device(int = -1);
	int handle_set_input_left(int = 0);
	int handle_set_output_left(int = 0);
	int handle_set_input_right(int = 0);
	int handle_set_output_right(int = 0);
	int handle_toggle_buffer_samples(int = -1);
	void handle_set_input_jitter(int);
	void handle_set_output_jitter(int);
};

class HpsJamConfigFormat : public HpsJamGroupBox {
	Q_OBJECT
public:
	HpsJamConfigFormat() {
		for (unsigned x = 0; x != HPSJAM_AUDIO_FORMAT_MAX; x++) {
			b[x].setFlat(x != 1);
			b[x].setText(QString(hpsjam_audio_format[x].descr));
			connect(&b[x], SIGNAL(released()), this, SLOT(handle_selection()));
			if (x == 0)
				gl.addWidget(b + x, 0, 0);
			else
				gl.addWidget(b + x, (x - 1) / 4, ((x - 1) % 4) + 1);
		}
		format = hpsjam_audio_format[1].format;
		selection = 1;
	};
	uint8_t format;
	uint8_t selection;
	HpsJamPushButton b[HPSJAM_AUDIO_FORMAT_MAX];
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

struct hpsjam_audio_levels {
	const char *descr;
	float gain;
};

extern const struct hpsjam_audio_levels hpsjam_audio_levels[HPSJAM_AUDIO_LEVELS_MAX];

class HpsJamConfigEffects : public HpsJamGroupBox {
	Q_OBJECT
public:
	HpsJamConfigEffects() {
		for (unsigned x = 0; x != HPSJAM_AUDIO_LEVELS_MAX; x++) {
			b[x].setFlat(x != 0);
			b[x].setText(hpsjam_audio_levels[x].descr);
			connect(&b[x], SIGNAL(released()), this, SLOT(handle_selection()));
			gl.addWidget(b + x, 0, x);
		}
		selection = 0;
		setCollapsed(true);
	};
	uint8_t selection;
	HpsJamPushButton b[HPSJAM_AUDIO_LEVELS_MAX];
	QString description;

	void titleRegen() {
		setTitle(description + QString(" :: %1").arg(hpsjam_audio_levels[selection].descr));
	};

	void setIndex(unsigned index) {
		for (unsigned x = 0; x != HPSJAM_AUDIO_LEVELS_MAX; x++) {
			b[x].setFlat(x != index);
			if (x != index || index == selection)
				continue;
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

class HpsJamConfigMixer : public HpsJamGroupBox {
	Q_OBJECT
public:
	HpsJamConfigMixer() {
		setTitle("Mixer configuration");
		gl.addWidget(new QLabel("Select maximum number of columns"), 0,0);
		gl.addWidget(&mixer_cols, 0,1);
		mixer_cols.setRange(1, HPSJAM_PEERS_MAX + 1);
		connect(&mixer_cols, SIGNAL(valueChanged(int)), this, SLOT(handle_selection()));
		setCollapsed(true);
	};
	QSpinBox mixer_cols;
public slots:
	void handle_selection();
};

class HpsJamLyricsFormat : public HpsJamGroupBox {
public:
	HpsJamLyricsFormat() : b_font_select(tr("Select font")) {
		gl.addWidget(&b_font_select, 0,0);
		setTitle(tr("Lyrics configuration"));
		setCollapsed(true);
	};
	HpsJamPushButton b_font_select;
};

class HpsJamConfig : public HpsJamTWidget {
	Q_OBJECT
public:
	HpsJamConfig() : gl(this) {
		up_fmt.description = tr("Uplink audio format");
		up_fmt.titleRegen();

		down_fmt.description = tr("Downlink audio format");
		down_fmt.titleRegen();

		effects.description = tr("Sound effects");
		effects.titleRegen();

		gl.addWidget(&up_fmt, 0,0,1,2);
		gl.addWidget(&down_fmt, 1,0,1,2);
		gl.addWidget(&audio_dev, 2,0,1,2);
		gl.addWidget(&effects, 3,0,1,2);
		gl.addWidget(&mixer, 4,0,1,1);
		gl.addWidget(&lyrics_fmt, 4,1,1,1);
		gl.setRowStretch(5,1);

		connect(&up_fmt, SIGNAL(valueChanged()), this, SLOT(handle_up_config()));
		connect(&down_fmt, SIGNAL(valueChanged()), this, SLOT(handle_down_config()));
		connect(&effects, SIGNAL(valueChanged()), this, SLOT(handle_effects_config()));
	};
	void keyPressEvent(QKeyEvent *);
	QGridLayout gl;
	HpsJamConfigMixer mixer;
	HpsJamConfigFormat up_fmt;
	HpsJamConfigFormat down_fmt;
	HpsJamDeviceSelection audio_dev;
	HpsJamConfigEffects effects;
	HpsJamLyricsFormat lyrics_fmt;
public slots:
	void handle_up_config();
	void handle_down_config();
	void handle_effects_config();
};

#endif		/* _HPSJAM_CONFIGDLG_H_ */

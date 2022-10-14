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

#ifndef _HPSJAM_MIXERDLG_H_
#define	_HPSJAM_MIXERDLG_H_

#include <QObject>
#include <QWidget>
#include <QPushButton>
#include <QGridLayout>
#include <QRect>
#include <QPoint>
#include <QSvgRenderer>
#include <QLabel>
#include <QScrollArea>
#include <QGroupBox>
#include <QSizePolicy>

#include "texture.h"

#include "eqdlg.h"

class HpsJamIcon : public HpsJamRWidget {
	Q_OBJECT
public:
	HpsJamIcon(const QString & = QString());

	QString fname;
	QSvgRenderer svg;
	bool sel;
	bool enabled;
	void updateBackground();
	void paintEvent(QPaintEvent *);
	void mouseReleaseEvent(QMouseEvent *);
	void setSelection(bool);
	void setEnabled(bool state) {
		if (enabled == state)
			return;
		enabled = state;
		updateBackground();
		update();
	};
signals:
	void selected();
};

class HpsJamSlider : public QWidget {
	Q_OBJECT
public:
	static constexpr unsigned dsize = 16;	/* dot size in pixels */

	HpsJamSlider();

	bool active;
	QRect target;
	QPoint start;
	float value;
	float pan;
	float level[2];
	int gain;

	void setValue(float);
	void setPan(float);
	void setGain(int);
	void adjustPan(float);
	void adjustGain(int);
	void setLevel(float, float);

	void paintEvent(QPaintEvent *);
        void mousePressEvent(QMouseEvent *);
        void mouseMoveEvent(QMouseEvent *);
        void mouseReleaseEvent(QMouseEvent *);
signals:
	void valueChanged();
};

class HpsJamPan : public QObject {
	Q_OBJECT
public:
	HpsJamPushButton b_l;
	HpsJamPushButton b_r;

	HpsJamPan() : b_l(QString("L")), b_r(QString("R")) {
		connect(&b_l, SIGNAL(released()), this, SLOT(handle_pan_left()));
		connect(&b_r, SIGNAL(released()), this, SLOT(handle_pan_right()));
	};

public slots:
	void handle_pan_left();
	void handle_pan_right();
signals:
	void valueChanged(int);
};

class HpsJamGain : public QObject {
	Q_OBJECT
public:
	HpsJamPushButton b_inc;
	HpsJamPushButton b_dec;

	HpsJamGain() :
	    b_inc(QString("+")), b_dec(QString("-")) {
		setValue(0);
		connect(&b_inc, SIGNAL(released()), this, SLOT(handle_gain_up()));
		connect(&b_dec, SIGNAL(released()), this, SLOT(handle_gain_down()));
	};

	void setValue(int x) {
		b_inc.setEnabled(x < 15);
		b_dec.setEnabled(x > -16);
	};

public slots:
	void handle_gain_up();
	void handle_gain_down();
signals:
	void valueChanged(int);
};

class HpsJamStrip : public HpsJamGroupBox {
	Q_OBJECT
public:
	HpsJamStrip();

	int id;

	QLabel w_name;
	HpsJamIcon w_icon;
	HpsJamGain w_gain;
	HpsJamPan w_pan;
	HpsJamSlider w_slider;
	HpsJamEqualizer w_eq;
	HpsJamPushButton b_eq;
	HpsJamPushButton b_inv;
	HpsJamPushButton b_mute;
	HpsJamPushButton b_solo;
	QString description;

	void init() {
		w_name.setText(QString());
		w_icon.svg.load(QByteArray());
		w_icon.update();
		HPSJAM_NO_SIGNAL(w_slider,setValue(1));
		HPSJAM_NO_SIGNAL(w_slider,setPan(0));
		HPSJAM_NO_SIGNAL(w_slider,setLevel(0,0));
		HPSJAM_NO_SIGNAL(w_slider,setGain(0));
		w_eq.handle_disable();
		HPSJAM_NO_SIGNAL(b_inv,setFlat(false));
		HPSJAM_NO_SIGNAL(b_mute,setFlat(false));
		HPSJAM_NO_SIGNAL(b_solo,setFlat(false));
		HPSJAM_NO_SIGNAL(w_gain,setValue(0));
		titleRegen();
	};

	void titleRegen() {
		QString bits;

		if (b_inv.isFlat())
			bits += "I";
		if (b_mute.isFlat())
			bits += "M";
		if (b_solo.isFlat())
			bits += "S";
		if (w_eq.edit.toPlainText().trimmed() != QString("filtersize 0.0ms 0.0ms"))
			bits += "E";
		if (w_slider.pan < 0.0f)
			bits += "L";
		else if (w_slider.pan > 0.0f)
			bits += "R";
		if (description == "Balance") {
			if (w_slider.value > 0.0f)
				bits += "G";
		} else {
			if (w_slider.value < 1.0f)
				bits += "G";
		}
		if (w_slider.gain != 0) {
			bits += QString("%1%2")
			  .arg(w_slider.gain > 0 ? "+" : "-")
			  .arg(w_slider.gain < 0 ? -w_slider.gain : w_slider.gain);
		}

		if (bits.isEmpty())
			setTitle(description);
		else
			setTitle(description + QString(" ") + bits);
	};

	uint8_t getBits() {
		uint8_t ret = HPSJAM_BIT_GAIN_SET(w_slider.gain);
		if (b_inv.isFlat())
			ret |= HPSJAM_BIT_INVERT;
		if (b_mute.isFlat())
			ret |= HPSJAM_BIT_MUTE;
		if (b_solo.isFlat())
			ret |= HPSJAM_BIT_SOLO;
		return (ret);
	};

public slots:
	void handleSlider();
	void handleSolo();
	void handleMute();
	void handleEQShow();
	void handleEQApply();
	void handleInv();
	void handlePan(int);
	void handleGain(int);

signals:
	void gainChanged(int);
	void panChanged(int);
	void bitsChanged(int);
	void eqChanged(int);
};

class HpsJamMixer : public QScrollArea {
	Q_OBJECT
public:
	HpsJamMixer() : gl(&w_main) {
		my_peer = 0;
		mixer_cols = HPSJAM_PEERS_MAX + 1;
		setWidgetResizable(true);

		self_strip.description = tr("Balance");
		self_strip.titleRegen();
		self_strip.b_solo.setEnabled(false);
		self_strip.w_gain.b_inc.setEnabled(false);
		self_strip.w_gain.b_dec.setEnabled(false);
		self_strip.w_icon.setSelection(true);

		connect(&self_strip, SIGNAL(eqChanged(int)), this, SLOT(handle_local_eq_changed()));
		self_strip.id = 0;
		gl.addWidget(&self_strip, 0, 0);
		for (unsigned x = 0; x != HPSJAM_PEERS_MAX; x++) {
			peer_strip[x].description = QString("Mix%1").arg(1 + x);
			peer_strip[x].titleRegen();
			peer_strip[x].id = x;
			peer_strip[x].hide();
			peer_strip[x].w_slider.setValue(1);

			addPeer(x);
			connect(peer_strip + x, SIGNAL(bitsChanged(int)), this, SLOT(handle_bits_changed(int)));
			connect(peer_strip + x, SIGNAL(gainChanged(int)), this, SLOT(handle_gain_changed(int)));
			connect(peer_strip + x, SIGNAL(panChanged(int)), this, SLOT(handle_pan_changed(int)));
			connect(peer_strip + x, SIGNAL(eqChanged(int)), this, SLOT(handle_eq_changed(int)));
		}
		setWidget(&w_main);
		viewport()->setAutoFillBackground(false);
		w_main.setAutoFillBackground(false);
	};
	void keyPressEvent(QKeyEvent *event);
	QWidget w_main;
	QGridLayout gl;
	HpsJamStrip self_strip;
	HpsJamStrip peer_strip[HPSJAM_PEERS_MAX];
	HpsJamStrip *my_peer;
	unsigned mixer_cols;
	void enable(unsigned index);
	void disable(unsigned index);
	void init() {
		my_peer = 0;
		for (unsigned x = 0; x != HPSJAM_PEERS_MAX; x++) {
			peer_strip[x].init();
			peer_strip[x].hide();
		}
	};
	void addPeer(unsigned x) {
		gl.addWidget(peer_strip + x, (1 + x) / mixer_cols, (1 + x) % mixer_cols);
	};
	unsigned getMixerCols() {
		return (mixer_cols);
	};
	void setMixerCols(unsigned _value) {
		/* check for invalid parameter */
		if (_value <= 0 || _value >= 65536)
			return;
		mixer_cols = _value;
		for (unsigned x = 0; x != HPSJAM_PEERS_MAX; x++)
			gl.removeWidget(peer_strip + x);
		for (unsigned x = 0; x != HPSJAM_PEERS_MAX; x++)
			addPeer(x);
	};

public slots:
	void handle_fader_level(uint8_t, uint8_t, float, float);
	void handle_fader_name(uint8_t, uint8_t, QString *);
	void handle_fader_icon(uint8_t, uint8_t, QByteArray *);
	void handle_fader_gain(uint8_t, uint8_t, float);
	void handle_fader_pan(uint8_t, uint8_t, float);
	void handle_fader_eq(uint8_t, uint8_t, QString *);
	void handle_fader_self(uint8_t, uint8_t);
	void handle_fader_disconnect(uint8_t, uint8_t);

	void handle_bits_changed(int);
	void handle_gain_changed(int);
	void handle_pan_changed(int);
	void handle_eq_changed(int);

	void handle_local_eq_changed();
};

#endif		/* _HPSJAM_MIXERDLG_H_ */

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

#include "hpsjam.h"

#include "eqdlg.h"

class HpsJamIcon : public QWidget {
public:
	HpsJamIcon();

	QSvgRenderer svg;

	void paintEvent(QPaintEvent *);
};

class HpsJamSlider : public QWidget {
	Q_OBJECT;
public:
	static constexpr unsigned dsize = 16;	/* dot size in pixels */

	HpsJamSlider();

	bool active;
	QRect target;
	QPoint start;
	float value;
	float pan;
	float level[2];

	void setValue(float);
	void setPan(float);
	void adjustPan(float);
	void setLevel(float, float);

	void paintEvent(QPaintEvent *);
        void mousePressEvent(QMouseEvent *);
        void mouseMoveEvent(QMouseEvent *);
        void mouseReleaseEvent(QMouseEvent *);
signals:
	void valueChanged();
};

class HpsJamPan : public QWidget {
	Q_OBJECT;
public:
	QGridLayout gl;
	QPushButton b[2];

	HpsJamPan() : gl(this) {
		int w = b[0].fontMetrics().boundingRect(QString("L_R")).width();
		b[0].setText(QString("L"));
		b[1].setText(QString("R"));
		b[0].setFixedWidth(w);
		b[1].setFixedWidth(w);
		connect(b + 0, SIGNAL(released()), this, SLOT(handle_pan_left()));
		connect(b + 1, SIGNAL(released()), this, SLOT(handle_pan_right()));
		gl.addWidget(b + 0,0,0);
		gl.addWidget(b + 1,0,1);
	};

public slots:
	void handle_pan_left();
	void handle_pan_right();
signals:
	void valueChanged(int);
};

class HpsJamStrip : public QGroupBox {
	Q_OBJECT;
public:
	HpsJamStrip();

	int id;

	QGridLayout gl;
	QLabel w_name;
	HpsJamIcon w_icon;
	HpsJamPan w_pan;
	HpsJamSlider w_slider;
	HpsJamEqualizer w_eq;
	QPushButton b_eq;
	QPushButton b_inv;
	QPushButton b_mute;
	QPushButton b_solo;

	void init() {
		w_name.setText(QString());
		w_icon.svg.load(QByteArray());
		HPSJAM_NO_SIGNAL(w_slider,setValue(1));
		HPSJAM_NO_SIGNAL(w_slider,setPan(0));
		HPSJAM_NO_SIGNAL(w_slider,setLevel(0,0));
		w_eq.handle_disable();
		HPSJAM_NO_SIGNAL(b_inv,setFlat(false));
		HPSJAM_NO_SIGNAL(b_mute,setFlat(false));
		HPSJAM_NO_SIGNAL(b_solo,setFlat(false));
	};

	uint8_t getBits() {
		uint8_t ret = 0;
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

signals:
	void gainChanged(int);
	void panChanged(int);
	void bitsChanged(int);
	void eqChanged(int);
};

class HpsJamMixer : public QScrollArea {
	Q_OBJECT;
public:
	HpsJamMixer() : gl(&w_main) {
		setWidgetResizable(true);
		self_strip.setTitle(QString("Local"));
		connect(&self_strip, SIGNAL(eqChanged(int)), this, SLOT(handle_local_eq_changed()));
		self_strip.id = 0;
		gl.addWidget(&self_strip, 0, 0);
		for (unsigned x = 0; x != HPSJAM_PEERS_MAX; x++) {
			peer_strip[x].setTitle(QString("Mix%1").arg(1 + x));
			peer_strip[x].id = x;
			peer_strip[x].hide();
			peer_strip[x].w_slider.setValue(1);

			gl.addWidget(peer_strip + x, (1 + x) / 8, (1 + x) % 8);
			connect(peer_strip + x, SIGNAL(bitsChanged(int)), this, SLOT(handle_bits_changed(int)));
			connect(peer_strip + x, SIGNAL(gainChanged(int)), this, SLOT(handle_gain_changed(int)));
			connect(peer_strip + x, SIGNAL(panChanged(int)), this, SLOT(handle_pan_changed(int)));
			connect(peer_strip + x, SIGNAL(eqChanged(int)), this, SLOT(handle_eq_changed(int)));
		}
		setWidget(&w_main);
	};
	QWidget w_main;
	QGridLayout gl;
	HpsJamStrip self_strip;
	HpsJamStrip peer_strip[HPSJAM_PEERS_MAX];
	void enable(unsigned index) {
		peer_strip[index].show();
	};
	void disable(unsigned index) {
		peer_strip[index].init();
		peer_strip[index].hide();
	};
	void init() {
		for (unsigned x = 0; x != HPSJAM_PEERS_MAX; x++) {
			peer_strip[x].init();
			peer_strip[x].hide();
		}
	};

public slots:
	void handle_fader_level(uint8_t, uint8_t, float, float);
	void handle_fader_name(uint8_t, uint8_t, QString *);
	void handle_fader_icon(uint8_t, uint8_t, QByteArray *);
	void handle_fader_gain(uint8_t, uint8_t, float);
	void handle_fader_pan(uint8_t, uint8_t, float);
	void handle_fader_eq(uint8_t, uint8_t, QString *);
	void handle_fader_disconnect(uint8_t, uint8_t);

	void handle_bits_changed(int);
	void handle_gain_changed(int);
	void handle_pan_changed(int);
	void handle_eq_changed(int);

	void handle_local_eq_changed();
};

#endif		/* _HPSJAM_MIXERDLG_H_ */

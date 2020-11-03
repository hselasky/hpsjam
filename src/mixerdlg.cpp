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

#include "mixerdlg.h"

#include <QMouseEvent>
#include <QPainter>

HpsJamIcon :: HpsJamIcon()
{

}

void
HpsJamIcon :: paintEvent(QPaintEvent *event)
{
	QPainter paint(this);
}

HpsJamSlider :: HpsJamSlider()
{
	target = QRect(0,0,0,0);
	start = QPoint(0,0);
	value = 0;
	level = 0;
	active = false;
}

void
HpsJamSlider :: setValue(float _value)
{
	if (_value != value) {
		value = _value;
		update();
		emit valueChanged();
	}
}

void
HpsJamSlider :: setLevel(float _level)
{
	if (_level != level) {
		level = _level;
		update();
	}
}

void
HpsJamSlider :: paintEvent(QPaintEvent *event)
{
	QPainter paint(this);
}

void
HpsJamSlider :: mousePressEvent(QMouseEvent *event)
{
	if (target.contains(event->pos())) {
		active = true;
		start = event->pos();
	}
}

void
HpsJamSlider :: mouseMoveEvent(QMouseEvent *event)
{
	if (active == true) {
		QPoint delta = event->pos() - start;
		float offset = (float)delta.x() / (float)height();
		start = event->pos();

		value += offset;
		if (value < 0.0f)
			value = 0.0f;
		else if (value > 1.0f)
			value = 1.0f;

		if (offset != 0.0f)
			emit valueChanged();
	}
}

void
HpsJamSlider :: mouseReleaseEvent(QMouseEvent *event)
{
	if (active) {
		QPoint delta = event->pos() - start;
		float offset = (float)delta.x() / (float)height();
		value += offset;
		if (value < 0.0f)
			value = 0.0f;
		else if (value > 1.0f)
			value = 1.0f;

		if (offset != 0.0f)
			emit valueChanged();

		active = false;
	}
}

HpsJamStrip :: HpsJamStrip(int _id)
{
	id = _id;
	gl = new QGridLayout(this);
	w_name = new QLabel();
	w_icon = new HpsJamIcon();
	w_slider = new HpsJamSlider();
	w_eq = new QPushButton(tr("EQ"));
	w_inv = new QPushButton(tr("INV"));
	w_solo = new QPushButton(tr("SOLO"));
	w_mute = new QPushButton(tr("MUTE"));

	connect(w_slider, SIGNAL(valueChanged()), this, SLOT(handleSlider()));
	connect(w_eq, SIGNAL(released()), this, SLOT(handleEQ()));
	connect(w_inv, SIGNAL(released()), this, SLOT(handleInv()));
	connect(w_solo, SIGNAL(released()), this, SLOT(handleSolo()));
	connect(w_mute, SIGNAL(released()), this, SLOT(handleMute()));

	gl->addWidget(w_icon, 0,0);
	gl->addWidget(w_name, 1,0);
	gl->addWidget(w_eq, 2,0);
	gl->addWidget(w_inv, 3,0);
	gl->addWidget(w_slider, 4,0);
	gl->setRowStretch(4,1);
	gl->addWidget(w_solo, 5,0);
	gl->addWidget(w_mute, 6,0);
}

void
HpsJamStrip :: handleEQ()
{

}

void
HpsJamStrip :: handleSlider()
{
	emit valueChanged(id);
}

void
HpsJamStrip :: handleInv()
{
	if (w_inv->isFlat())
		w_inv->setFlat(false);
	else
		w_inv->setFlat(true);

	emit valueChanged(id);
}

void
HpsJamStrip :: handleSolo()
{
	if (w_solo->isFlat())
		w_solo->setFlat(false);
	else
		w_solo->setFlat(true);

	emit valueChanged(id);
}

void
HpsJamStrip :: handleMute()
{
	if (w_mute->isFlat())
		w_mute->setFlat(false);
	else
		w_mute->setFlat(true);

	emit valueChanged(id);
}

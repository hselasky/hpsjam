/*-
 * Copyright (c) 2012-2020 Hans Petter Selasky. All rights reserved.
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

#include <QString>
#include <QPainter>
#include <QMouseEvent>

#include "volumedlg.h"

HpsJamVolume :: HpsJamVolume()
{
	y_pos = moving = focus = curr_delta =
	    curr_pos = min = max = 0;
	mid = 1;

	setMinimumSize(QSize(50,50));
	setMaximumSize(QSize(50,50));
}

void
HpsJamVolume :: mousePressEvent(QMouseEvent *event)
{
	if (event->button() == Qt::LeftButton) {
		moving = 1;
		curr_delta = 0;
		y_pos = event->y();
	}
}

void
HpsJamVolume :: mouseReleaseEvent(QMouseEvent *event)
{
	if (event->button() == Qt::LeftButton) {
		moving = 0;
		y_pos = 0;
		curr_pos += curr_delta;
		curr_delta = 0;

		emit valueChanged(curr_pos);
		update();
	}
}

void
HpsJamVolume :: mouseMoveEvent(QMouseEvent *event)
{
	if (moving) {
		curr_delta = ((y_pos - event->y()) * (max - min + 1)) / 128;

		if ((curr_delta + curr_pos) > max)
			curr_delta = max - curr_pos;
		else if ((curr_delta + curr_pos) < min)
			curr_delta = min - curr_pos;

		emit valueChanged(curr_pos + curr_delta);

		update();
	}
}

void
HpsJamVolume :: setRange(int from, int to, int middle)
{
	curr_pos = from;
	min = from;
	max = to;
	mid = middle;
}

int
HpsJamVolume :: value(void) const
{
	return (curr_pos + curr_delta);
}

void
HpsJamVolume :: setValue(int value)
{
	if (value > max)
		curr_pos = max;
	else if (value < min)
		curr_pos = min;
	else
		curr_pos = value;

	curr_delta = 0;

	emit valueChanged(curr_pos);

	update();
}

void
HpsJamVolume :: enterEvent(QEvent *event)
{
	focus = 1;
	update();
}

void
HpsJamVolume :: leaveEvent(QEvent *event)
{
	focus = 0;
	update();
}

void
HpsJamVolume :: paintEvent(QPaintEvent *event)
{
	QPainter paint(this);
	QFont fnt;
	const int m = 4;
	int w = width();
	int h = height();
	int val = value() - min;
	int div = mid - min;
	int angle = (val * 270 * 16) / (max - min + 1);
	int color = (val * (255 - 128)) / (max - min + 1);

	paint.setRenderHints(QPainter::Antialiasing, 1);

	QColor black(0,0,0);
	QColor button(128,128,128);
	QColor button_focus(128,192,128);
	QColor active(128+color,128,128);
	QColor background(0,0,0,0);

	fnt.setPixelSize(3*m);

	if (div == 0)
		div = 1;

	QString descr = QString("%1").arg((double)val/(double)div, 0, 'f', 2);

	paint.fillRect(QRect(0,0,w,h), background);

	paint.setPen(QPen(black, 1));
	paint.setBrush(active);
	paint.drawPie(QRect(m,m,w-(2*m),h-(2*m)),(180+45)*16, -angle);

	if (focus)
		paint.setBrush(button_focus);
	else
		paint.setBrush(button);

	paint.drawEllipse(QRect((w/4)+m,(h/4)+m,(w/2)-(2*m), (h/2)-(2*m)));

	paint.setFont(fnt);

	QRectF sz = paint.boundingRect(QRect(0,0,0,0), descr);

	paint.drawText(QPointF((w - sz.width()) / 2.0, h), descr);
}

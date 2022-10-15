/*-
 * Copyright (c) 2022 Hans Petter Selasky.
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

#include "texture.h"

#include <QPainter>

static void
round_corners(QImage *p, int r)
{
	const QSize s(p->size());

	r = qMin(r, qMin(s.width(),s.height()));
	if (r <= 1)
		return;

	const int zz = r * r;

	for (int x = 0; x != r; x++) {
		const int xx = x * x;

		for (int y = r; y--; ) {
			const int yy = y * y;

			const QPoint pos[4] = {
				QPoint(r - 1 - x, r - 1 - y),
				QPoint(s.width() - 1 - (r - 1 - x), s.height() - 1 - (r - 1 - y)),
				QPoint(r - 1 - x, s.height() - 1 - (r - 1 - y)),
				QPoint(s.width() - 1 - (r - 1 - x), r - 1 - y),
			};

			if (xx + yy > zz) {
				p->setPixel(pos[0],0);
				p->setPixel(pos[1],0);
				p->setPixel(pos[2],0);
				p->setPixel(pos[3],0);
			} else {
				QColor cc[4] = {
					p->pixelColor(pos[0]),
					p->pixelColor(pos[1]),
					p->pixelColor(pos[2]),
					p->pixelColor(pos[3]),
				};

				cc[0].setAlpha(cc[0].alpha() / 2);
				cc[1].setAlpha(cc[1].alpha() / 2);
				cc[2].setAlpha(cc[2].alpha() / 2);
				cc[3].setAlpha(cc[3].alpha() / 2);

				p->setPixelColor(pos[0],cc[0]);
				p->setPixelColor(pos[1],cc[1]);
				p->setPixelColor(pos[2],cc[2]);
				p->setPixelColor(pos[3],cc[3]);

				break;
			}
		}
	}
}

void
HpsJamTexture :: paintEvent(QWidget *w, QPaintEvent *event)
{
	static QImage texture;
	const QSize size = w->size();

	if (s.width() != size.width() ||
	    s.height() != size.height() || rgb != last) {
		delete p;
		p = 0;
		s = size;
		last = rgb;
	}

	if (texture.isNull())
		texture = QImage(QString(":/icons/texture.png"));

	if (s.width() <= 0 || s.height() <= 0 || texture.isNull())
		return;

	if (p == 0) {
		p = new QImage(s.width(), s.height(), QImage::Format_ARGB32);
		if (p == 0)
			return;

		QImage tt(texture.width(), texture.height(), QImage::Format_ARGB32);

		for (int x = 0; x != tt.width(); x++) {
			for (int y = 0; y != tt.height(); y++) {
				QColor cc(texture.pixelColor(x,y));

				cc.setRed(cc.red() * rgb.red() / 255);
				cc.setGreen(cc.green() * rgb.green() / 255);
				cc.setBlue(cc.blue() * rgb.blue() / 255);

				tt.setPixelColor(x,y,cc);
			}
		}

		QPainter paint(p);

		for (int w = 0; w < s.width(); w += texture.width()) {
			for (int h = 0; h < s.height(); h += texture.height()) {
				paint.drawImage(QRect(w,h,texture.width(),texture.height()),
						tt,
						QRect(0,0,texture.width(),texture.height()));
			}
		}

		paint.end();

		round_corners(p, r);
	}

	QPainter paint(w);

	paint.setClipRegion(event->region());
	paint.drawImage(0,0,*p);
	paint.end();
};

void
HpsJamRounded :: paintEvent(QWidget *w, QPaintEvent *event)
{
	const QSize size = w->size();

	if (s.width() != size.width() ||
	    s.height() != size.height() || rgb != last) {
		delete p;
		p = 0;
		s = size;
		last = rgb;
	}
	if (s.width() <= 0 || s.height() <= 0)
		return;

	if (p == 0) {
		p = new QImage(s.width(), s.height(), QImage::Format_ARGB32);
		if (p == 0)
			return;

		p->fill(rgb);

		round_corners(p, r);
	}

	QPainter paint(w);

	paint.setClipRegion(event->region());
	paint.drawImage(0,0,*p);
	paint.end();
};

HpsJamTWidget :: HpsJamTWidget() :
    t(palette().window().color(), 24)
{

}

void
HpsJamTWidget :: paintEvent(QPaintEvent *event)
{
	t.paintEvent(this, event);

	QWidget::paintEvent(event);
}

HpsJamRWidget :: HpsJamRWidget() :
    t(QColor(192,192,255,64), 24)
{

}

void
HpsJamRWidget :: paintEvent(QPaintEvent *event)
{
	t.paintEvent(this, event);

	QWidget::paintEvent(event);
}

HpsJamPushButton :: HpsJamPushButton(const QString &str) :
    t(QColor(255,255,255), 12)
{
	_flat = false;
	_flash = false;

	setText(str);
	setMinimumSize(1,1);
	setMaximumSize(65535,65535);
}

void
HpsJamPushButton :: paintEvent(QPaintEvent *event)
{
	if (isEnabled()) {
		if ((isFlat() || isDown()) ^ _flash)
			t.rgb.setAlpha(255);
		else
			t.rgb.setAlpha(127);
	} else {
		t.rgb.setAlpha(0);
	}
	t.paintEvent(this, event);

	QPainter paint(this);
	QString str(text());
	QRect bound = paint.boundingRect(
	    QRect(0,0,65535,65535), Qt::AlignLeft | Qt::AlignTop | Qt::TextShowMnemonic, str);

	paint.setPen(Qt::black);
	paint.setFont(font());
	paint.drawText(QRect(QPoint(0,0),size()),
	    Qt::AlignCenter | Qt::TextShowMnemonic, str);

	QSize sz = bound.size() + QSize(2 * t.r, 2 * t.r);

	setMinimumSize(sz);
}

HpsJamGroupBox :: HpsJamGroupBox() :
     t(QColor(255,255,255,127), 24), gl_inner(this), gl(&w)
{
	collapsed = false;
	l.t.r = 12;
	l.t.rgb = QColor(127,255,127,64);

	gl_inner.addWidget(&l, 0,0,1,1);
	gl_inner.addWidget(&w, 1,0,1,1);
	gl_inner.setRowStretch(1,1);

	connect(&l, SIGNAL(released()), this, SLOT(handle_toggle_collapsed()));
}

void
HpsJamGroupBox :: paintEvent(QPaintEvent *event)
{
	t.paintEvent(this, event);

	QWidget::paintEvent(event);
}

void
HpsJamGroupBox :: setCollapsed(bool _value)
{
	if (collapsed == _value)
		return;
	collapsed = _value;
	if (_value)
		w.hide();
	else
		w.show();
}

void
HpsJamGroupBox :: handle_toggle_collapsed()
{
	setCollapsed(!collapsed);
}

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

#ifndef _HPSJAM_TEXTURE_H_
#define	_HPSJAM_TEXTURE_H_

#include "hpsjam.h"

#include <QAbstractButton>
#include <QColor>
#include <QGridLayout>
#include <QGroupBox>
#include <QImage>
#include <QLabel>
#include <QPaintEvent>
#include <QSize>
#include <QWidget>

class HpsJamTexture {
	QImage *p;
	QSize s;
	QColor last;
public:
	int r;
	QColor rgb;

	HpsJamTexture(const QColor &_rgb, int _r) {
		p = 0;
		last = rgb = _rgb;
		r = _r;
		s = QSize(0,0);
	};
	~HpsJamTexture() {
		delete p;
	};

	void paintEvent(QWidget *, QPaintEvent *);
};

class HpsJamRounded {
public:
	int r;
	QColor rgb;

	HpsJamRounded(const QColor &_rgb, int _r) {
		rgb = _rgb;
		r = _r;
	};

	void paintEvent(QWidget *, QPaintEvent *);
};

class HpsJamTWidget : public QWidget {
public:
	HpsJamTexture t;
	HpsJamTWidget();

	void paintEvent(QPaintEvent *);
};

class HpsJamRWidget : public QWidget {
public:
	HpsJamRounded t;
	HpsJamRWidget();

	void paintEvent(QPaintEvent *);
};

class HpsJamPushButton : public QAbstractButton {
public:
	HpsJamRounded t;
	bool _flat;
	bool _flash;

	HpsJamPushButton(const QString & = QString());

	void paintEvent(QPaintEvent *);

	bool isFlat() const { return (_flat); };
	void setFlat(bool _value) {
		if (_flat == _value)
			return;
		_flat = _value;
		update();
	};

	void setText(const QString &str) {
		QAbstractButton::setText(str);
		update();
	};
};

class HpsJamGroupBox : public QWidget {
	Q_OBJECT
public:
	HpsJamRounded t;
	HpsJamPushButton l;
	QWidget w;
	QGridLayout gl_inner;
	QGridLayout gl;
	bool collapsed;

	HpsJamGroupBox();

	void paintEvent(QPaintEvent *);
	void setTitle(const QString &str) { l.setText(str); };
	QString title() const { return (l.text()); };
	void setCollapsed(bool _value);

public slots:
	void handle_toggle_collapsed();
};

#endif		/* _HPSJAM_TEXTURE_H_ */

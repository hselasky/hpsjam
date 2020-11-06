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

#ifndef _HPSJAM_LYRICSDLG_H_
#define	_HPSJAM_LYRICSDLG_H_

#include <stdbool.h>

#include <QTimer>
#include <QWidget>
#include <QString>
#include <QFont>

#define	HPSJAM_TRAN_MAX 5

class HpsJamLyricsAnim {
public:
	QString str;
	float opacity_curr;
	float opacity_step;
	float xpos_curr;
	float xpos_step;
	float ypos_curr;
	float ypos_step;
	float width;
	float height;
	uint8_t currStep;

	void reset()
	{
		opacity_curr = 0.0f;
		opacity_step = 0.0f;
		xpos_curr = 0.0f;
		xpos_step = 0.0f;
		ypos_curr = 0.0f;
		ypos_step = 0.0f;
		width = 0.0f;
		height = 0.0f;
		currStep = 0;
		str = QString();
	}
	HpsJamLyricsAnim()
	{
		reset();
		currStep = HPSJAM_TRAN_MAX;
	}
	bool step()
	{
		if (currStep >= HPSJAM_TRAN_MAX) {
			/* animation finished */
			opacity_step = 0;
			xpos_step = 0;
			ypos_step = 0;
			currStep++;
			return (false);
		}
		/* animation in progress */
		opacity_curr += opacity_step;
		xpos_curr += xpos_step;
		ypos_curr += ypos_step;
		currStep++;
		return (true);
	};
	void fadeOut()
	{
		opacity_step = -(opacity_curr / HPSJAM_TRAN_MAX);
		currStep = 0;
	}
	void fadeIn()
	{
		opacity_step = ((1.0 - opacity_curr) / HPSJAM_TRAN_MAX);
		currStep = 0;
	}
	void moveUp(float pix)
	{
		if (pix != 0.0f) {
			ypos_step = -(pix / HPSJAM_TRAN_MAX);
			currStep = 0;
		}
	}
	bool isVisible()
	{
		return ((ypos_curr + height) >= 0.0f &&
		    (opacity_curr >= (1.0 / 1024.0)));
	}
	bool isAnimating()
	{
		return (currStep <= HPSJAM_TRAN_MAX);
	}
};

class HpsJamLyrics : public QWidget {
	Q_OBJECT;
public:
	enum { maxIndex = 3 };

	HpsJamLyrics() {
		index = 0;
		customFont = false;
		connect(&watchdog, SIGNAL(timeout()), this, SLOT(handle_watchdog()));
		watchdog.start(1000 / (3 * HPSJAM_TRAN_MAX));
	};
	uint8_t index;
	HpsJamLyricsAnim anim[maxIndex];
	bool customFont;
	QFont font;
	QTimer watchdog;

	void paintEvent(QPaintEvent *);
	void keyPressEvent(QKeyEvent *);
	void mouseDoubleClickEvent(QMouseEvent *);
	void append(const QString &);

public slots:
	void handle_fullscreen();
	void handle_watchdog();
	void handle_font_dialog();
};

#endif		/* _HPSJAM_LYRICSDLG_H_ */

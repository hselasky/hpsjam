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

#ifndef _HPSJAM_LYRICSDLG_H_
#define	_HPSJAM_LYRICSDLG_H_

#include "texture.h"

#include <stdbool.h>

#include <QTimer>
#include <QWidget>
#include <QString>
#include <QFont>

#define	HPSJAM_TRAN_MAX 5

class HpsJamLyricsAnim {
public:
	QString str;
	float height;

	void reset()
	{
		height = 0.0f;
		str = QString();
	}

	HpsJamLyricsAnim()
	{
		reset();
	}
};

class HpsJamLyrics : public HpsJamTWidget {
	Q_OBJECT;
public:
	enum { maxIndex = 3 };

	HpsJamLyrics() {
		needupdate = false;
		step = 0;
		index = 0;
		draw_index = 0;
		customFont = false;
		connect(&watchdog, SIGNAL(timeout()), this, SLOT(handle_watchdog()));
		watchdog.start(1000 / (3 * HPSJAM_TRAN_MAX));
	};
	uint8_t step;
	uint8_t index;
	uint8_t draw_index;
	HpsJamLyricsAnim anim[maxIndex];
	bool customFont;
	bool needupdate;
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

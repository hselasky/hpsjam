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

#include <QKeyEvent>
#include <QFontDialog>
#include <QPainter>

#include "hpsjam.h"
#include "clientdlg.h"
#include "lyricsdlg.h"

void
HpsJamLyrics :: paintEvent(QPaintEvent *event)
{
	QPainter paint(this);
	int w = width();
	int h = height();

	static const QColor bg(0,0,0);
	static const QColor fg(255,255,255);

	if (customFont == false)
		font.setPixelSize(w / 16);

	HpsJamLyricsAnim *obj[3] = {
		&anim[(draw_index + 0) % maxIndex],
		&anim[(draw_index + 1) % maxIndex],
		&anim[(draw_index + 2) % maxIndex],
	};

	paint.setRenderHints(QPainter::Antialiasing, 1);

	while (1) {
		paint.fillRect(QRectF(0,0,w,h), bg);

		float y_pos = - (obj[0]->height * step) / HPSJAM_TRAN_MAX;

		paint.setFont(font);

		for (uint8_t x = 0; x != maxIndex; x++) {

			const float wf = font.pixelSize();
			int flags = Qt::TextWordWrap | Qt::TextDontClip | Qt::AlignTop;

			QRectF txtBound;
			QRectF txtMax(0,0,w,h);

			paint.drawText(txtMax, Qt::AlignLeft |
			    Qt::TextDontPrint | flags, obj[x]->str, &txtBound);
			txtMax.setHeight(txtBound.height() + wf);

			flags |= Qt::AlignHCenter | Qt::AlignVCenter;

			/* offset bounding box */
			txtMax.adjust(0, y_pos + wf / 2.0f, 0, y_pos + wf / 2.0f);

			/* draw text */
			paint.setPen(fg);
			paint.setBrush(fg);
			paint.setOpacity((x == 2) ? (float)step / (float)HPSJAM_TRAN_MAX : 1.0f);
			paint.drawText(txtMax, flags, obj[x]->str);

			/* store height */
			obj[x]->height = txtMax.height();

			/* get next position */
			y_pos += obj[x]->height;
		}

		/* check if we can reuse already computed heights */
		if (needupdate == false)
			break;
		needupdate = false;
	}
}

void
HpsJamLyrics :: keyPressEvent(QKeyEvent *key)
{
	if (key->key() == Qt::Key_Escape) {
		if (hpsjam_client->windowState() & Qt::WindowFullScreen)
			handle_fullscreen();
	}
}

void
HpsJamLyrics :: mouseDoubleClickEvent(QMouseEvent *e)
{
	handle_fullscreen();
}

void
HpsJamLyrics :: handle_fullscreen()
{
	hpsjam_client->setWindowState(
	    hpsjam_client->windowState() ^ Qt::WindowFullScreen);
}

void
HpsJamLyrics :: handle_watchdog()
{
	if (step < HPSJAM_TRAN_MAX) {
		step++;
		update();
	} else if (draw_index != index) {
		draw_index = (draw_index + 1) % maxIndex;
		step = 0;
		update();
	}
}

void
HpsJamLyrics :: handle_font_dialog()
{
	bool success;

	QFont temp = QFontDialog::getFont(&success, font, this);

	if (success) {
		temp.setPixelSize(QFontInfo(temp).pixelSize());
		font = temp;
		customFont = true;
		update();
	} else {
		customFont = false;
	}
}

void
HpsJamLyrics :: append(const QString &str)
{
	anim[index].reset();
	anim[index].str = str;

	index = (index + 1) % maxIndex;
	needupdate = true;

	if (!isVisible())
		hpsjam_client->b_lyrics.setFlashing();
}

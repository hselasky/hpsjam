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

	QColor bg(0,0,0);
	QColor fg(255,255,255);

	paint.setRenderHints(QPainter::Antialiasing, 1);
	paint.fillRect(QRectF(0,0,w,h), bg);

	if (customFont == false)
		font.setPixelSize(w / 16);

	float y_pos = 0;

	for (uint8_t x = 0; x != maxIndex; x++) {
		HpsJamLyricsAnim &aobj = anim[(index + x) % maxIndex];

		if (aobj.isVisible() == false)
			continue;

		if (aobj.ypos_curr == 0.0f)
			aobj.ypos_curr = y_pos;
		else
			y_pos = aobj.ypos_curr;

		paint.setFont(font);

		float wf = font.pixelSize();
		int flags = Qt::TextWordWrap | Qt::TextDontClip | Qt::AlignTop;

		QRectF txtBound;
		QRectF txtMax(0,0,w,h);

		paint.drawText(txtMax, Qt::AlignLeft |
		    Qt::TextDontPrint | flags, aobj.str, &txtBound);
		txtMax.setHeight(txtBound.height() + wf);

		flags |= Qt::AlignHCenter | Qt::AlignVCenter;

		/* offset bounding box */
		txtMax.adjust(
		    aobj.xpos_curr,
		    aobj.ypos_curr + wf / 2.0f,
		    aobj.xpos_curr,
		    aobj.ypos_curr + wf / 2.0f);

		/* draw text */
		paint.setPen(fg);
		paint.setBrush(fg);
		paint.setOpacity(aobj.opacity_curr);
		paint.drawText(txtMax, flags, aobj.str);

		/* store height and width */
		aobj.height = txtMax.height();
		aobj.width = txtMax.width();

		/* get next position */
		y_pos += aobj.height;
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
	for (uint8_t x = 0; x != maxIndex; x++) {
		if (anim[x].step())
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
	}
}

void
HpsJamLyrics :: append(const QString &str)
{
	const uint8_t n = (index + 1) % maxIndex;
	const uint8_t p = (maxIndex + index - 1) % maxIndex;

	anim[index].reset();
	anim[index].str = str;
	anim[index].fadeIn();
	anim[index].ypos_curr = anim[p].ypos_curr + anim[p].height;

	const float shift = anim[n].ypos_curr + anim[n].height;

	/* shift rest of text blocks up */
	for (uint8_t x = 0; x != maxIndex; x++)
		anim[x].moveUp(shift);

	/* advance index */
	index = (index + 1) % maxIndex;

	if (!isVisible())
		hpsjam_client->b_lyrics.setFlashing();
}

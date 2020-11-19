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

#include <QMutexLocker>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPainter>

#include "hpsjam.h"
#include "peer.h"
#include "mixerdlg.h"
#include "protocol.h"
#include "clientdlg.h"

HpsJamIcon :: HpsJamIcon(const QString &_fname) :
    fname(_fname), sel(0)
{
	enabled = true;
	if (!fname.isEmpty())
		svg.load(fname);
	setFixedSize(64,64);
}

void
HpsJamIcon :: mouseReleaseEvent(QMouseEvent *event)
{
	if (event->button() != Qt::LeftButton)
		return;
	if (enabled)
		emit selected();
}

void
HpsJamIcon :: paintEvent(QPaintEvent *event)
{
	QPainter paint(this);

	paint.setRenderHints(QPainter::Antialiasing, 1);

	static const QColor gh(192,192,192,127);
	static const QColor gg(192,192,192);
	static const QColor bg(255,255,255);

	switch (sel) {
	case 0:
		paint.fillRect(QRect(0,0,width(),height()), gh);
		break;
	case 1:
		paint.fillRect(QRect(0,0,width(),height()), gg);
		break;
	case 2:
		paint.fillRect(QRect(0,0,width(),height()), bg);
		break;
	default:
		break;
	}
	svg.render(&paint);

	if (!enabled)
		paint.fillRect(QRect(0,0,width(),height()), gh);
}

void
HpsJamIcon :: setSelection(bool state)
{
	uint8_t _sel = state + 1;
	if (sel == _sel)
		return;
	sel = _sel;
	update();
	emit selected();
}

HpsJamSlider :: HpsJamSlider()
{
	target = QRect(0,0,0,0);
	start = QPoint(0,0);
	value = 0;
	pan = 0;
	gain = 0;
	level[0] = 0;
	level[1] = 0;
	active = false;
	setMinimumWidth(dsize);
	setMinimumHeight(128);
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
HpsJamSlider :: setPan(float _pan)
{
	if (_pan != pan) {
		pan = _pan;
		update();
	}
}

void
HpsJamSlider :: setGain(int _gain)
{
	if (_gain != gain) {
		gain = _gain;
		update();
	}
}

void
HpsJamSlider :: adjustPan(float _pan)
{
	pan += _pan;

	if (pan > 1.0f)
		pan = 1.0f;
	else if (pan < -1.0f)
		pan = -1.0f;

	if (_pan != 0.0f)
		update();
}

void
HpsJamSlider :: setLevel(float _left, float _right)
{
	if (_left != level[0] || _right != level[1]) {
		level[0] = _left;
		level[1] = _right;
		update();
	}
}

void
HpsJamSlider :: adjustGain(int delta)
{
	if (delta == 0)
		return;

	int temp = gain + delta;

	/* range check value */
	if (temp < -16)
		temp = -16;
	else if (temp > 15)
		temp = 15;

	gain = temp;
	update();
}

void
HpsJamSlider :: paintEvent(QPaintEvent *event)
{
	QRect frame(0, 0, width(), height());

	QPainter paint(this);

	paint.setRenderHints(QPainter::Antialiasing, 1);

	/* select foreground color */
	static const QColor fg(0, 0, 0);

	/* select background color */
	static const QColor bg(255, 255, 255);

	/* select gain color */
	static const QColor gc(192, 192, 192);

	static const QColor lc[3] = {
		QColor(0, 255, 0),
		QColor(255, 255, 0),
		QColor(255, 0, 0),
	};

	paint.fillRect(frame, bg);

	const unsigned dots = height() / dsize;
	if (dots < 2)
		return;

	/* compute height of gain bar */
	int gh = height() - (height() * (gain + 16)) / 31;
	paint.fillRect(QRect(0, height() / 2 - 2, width(), 1), gc);
	paint.fillRect(QRect(0, gh, dsize, height() - gh), gc);
	paint.fillRect(QRect(0, gh, dsize, 2), fg);

	const unsigned dist[3] = {
		dots - (dots + 7) / 8 - (dots + 4) / 5,
		dots - (dots + 7) / 8,
		dots,
	};

	unsigned ldots = dots * level[0];
	unsigned y;

	for (unsigned x = y = 0; x != ldots; x++) {
		while (dist[y] == x)
			y++;
		paint.setPen(QPen(QBrush(lc[y]), dsize,
		    Qt::SolidLine, Qt::RoundCap));
		paint.drawPoint(QPoint(width() / 2 - dsize / 2, height() - x * dsize - dsize / 2));
	}

	ldots = dots * level[1];

	for (unsigned x = y = 0; x != ldots; x++) {
		while (dist[y] == x)
			y++;
		paint.setPen(QPen(QBrush(lc[y]), dsize,
		    Qt::SolidLine, Qt::RoundCap));
		paint.drawPoint(QPoint(width() / 2 + dsize / 2, height() - x * dsize - dsize / 2));
	}

	target = QRect(2, (1.0f - value) * (height() - dsize), width() - 4, dsize);

	paint.setPen(QPen(QBrush(fg), 2));
	paint.drawRect(target);

	int p_off = pan * ((target.width() - dsize) / 2);

	QRect circle((target.width() - dsize) / 2 + p_off,
		     (1.0f - value) * (height() - dsize),
		     dsize, dsize);
	paint.setBrush(QBrush(p_off ? bg : fg));
	paint.drawEllipse(circle);
}

void
HpsJamSlider :: mousePressEvent(QMouseEvent *event)
{
	if (event->button() != Qt::LeftButton)
		return;
	if (target.contains(event->pos())) {
		active = true;
		start = event->pos();
	}
}

void
HpsJamSlider :: mouseMoveEvent(QMouseEvent *event)
{
	if (height() < (int)dsize)
		return;
	if (active == true) {
		const QPoint delta = event->pos() - start;
		const float offset = (float)delta.y() / (float)(height() - dsize);

		start = event->pos();

		value -= offset;
		if (value < 0.0f)
			value = 0.0f;
		else if (value > 1.0f)
			value = 1.0f;

		if (offset != 0.0f) {
			emit valueChanged();
			update();
		}
	}
}

void
HpsJamSlider :: mouseReleaseEvent(QMouseEvent *event)
{
	if (event->button() != Qt::LeftButton)
		return;

	if (active) {
		mouseMoveEvent(event);
		active = false;
	}
}

void
HpsJamPan :: handle_pan_left()
{
	emit valueChanged(-1);
}

void
HpsJamPan :: handle_pan_right()
{
	emit valueChanged(+1);
}

void
HpsJamGain :: handle_gain_up()
{
	emit valueChanged(+1);
}

void
HpsJamGain :: handle_gain_down()
{
	emit valueChanged(-1);
}

HpsJamStrip :: HpsJamStrip() : gl(this),
    b_eq(tr("EQ\nDELAY")),
    b_inv(tr("INV")),
    b_mute(tr("MUTE")),
    b_solo(tr("SOLO"))
{
	id = -1;

	setMaximumWidth(128);

	connect(&w_gain, SIGNAL(valueChanged(int)), this, SLOT(handleGain(int)));
	connect(&w_pan, SIGNAL(valueChanged(int)), this, SLOT(handlePan(int)));
	connect(&w_slider, SIGNAL(valueChanged()), this, SLOT(handleSlider()));
	connect(&b_eq, SIGNAL(released()), this, SLOT(handleEQShow()));
	connect(&w_eq.b_apply, SIGNAL(released()), this, SLOT(handleEQApply()));
	connect(&b_inv, SIGNAL(released()), this, SLOT(handleInv()));
	connect(&b_solo, SIGNAL(released()), this, SLOT(handleSolo()));
	connect(&b_mute, SIGNAL(released()), this, SLOT(handleMute()));

	gl.addWidget(&w_icon, 0,0, Qt::AlignCenter);
	gl.addWidget(&w_name, 1,0, Qt::AlignCenter);
	gl.addWidget(&b_eq, 2,0);
	gl.addWidget(&b_inv, 3,0);
	gl.addWidget(&w_gain, 4,0);
	gl.addWidget(&w_pan, 5,0);
	gl.addWidget(&w_slider, 6,0);
	gl.setRowStretch(6,1);
	gl.addWidget(&b_solo, 7,0);
	gl.addWidget(&b_mute, 8,0);
}

void
HpsJamStrip :: handleEQShow()
{
	w_eq.setWindowTitle(QString("HPS JAM equalizer for ") + title());
	w_eq.show();
	w_eq.edit.setFocus();
}

void
HpsJamStrip :: handleEQApply()
{
	emit eqChanged(id);
}

void
HpsJamStrip :: handleSlider()
{
	emit gainChanged(id);
}

void
HpsJamStrip :: handlePan(int delta)
{
	w_slider.adjustPan(delta / 16.0f);

	emit panChanged(id);
}

void
HpsJamStrip :: handleGain(int delta)
{
	w_slider.adjustGain(delta);
	w_gain.setValue(w_slider.gain);

	emit bitsChanged(id);
}

void
HpsJamStrip :: handleInv()
{
	if (b_inv.isFlat())
		b_inv.setFlat(false);
	else
		b_inv.setFlat(true);

	emit bitsChanged(id);
}

void
HpsJamStrip :: handleSolo()
{
	if (b_solo.isFlat())
		b_solo.setFlat(false);
	else
		b_solo.setFlat(true);

	emit bitsChanged(id);
}

void
HpsJamStrip :: handleMute()
{
	if (b_mute.isFlat())
		b_mute.setFlat(false);
	else
		b_mute.setFlat(true);

	emit bitsChanged(id);
}

void
HpsJamMixer :: keyPressEvent(QKeyEvent *event)
{
	switch (event->key()) {
	case Qt::Key_L:
		self_strip.w_pan.b[0].animateClick();
		break;
	case Qt::Key_R:
		self_strip.w_pan.b[1].animateClick();
		break;
	case Qt::Key_M:
		self_strip.b_mute.animateClick();
		break;
	case Qt::Key_I:
		self_strip.b_inv.animateClick();
		break;
	case Qt::Key_1:
		self_strip.w_slider.setValue(0.0f / 8.0f);
		break;
	case Qt::Key_2:
		self_strip.w_slider.setValue(1.0f / 8.0f);
		break;
	case Qt::Key_3:
		self_strip.w_slider.setValue(2.0f / 8.0f);
		break;
	case Qt::Key_4:
		self_strip.w_slider.setValue(3.0f / 8.0f);
		break;
	case Qt::Key_5:
		self_strip.w_slider.setValue(4.0f / 8.0f);
		break;
	case Qt::Key_6:
		self_strip.w_slider.setValue(5.0f / 8.0f);
		break;
	case Qt::Key_7:
		self_strip.w_slider.setValue(6.0f / 8.0f);
		break;
	case Qt::Key_8:
		self_strip.w_slider.setValue(7.0f / 8.0f);
		break;
	case Qt::Key_9:
		self_strip.w_slider.setValue(8.0f / 8.0f);
		break;
	case Qt::Key_E:
		self_strip.b_eq.animateClick();
		break;
	case Qt::Key_P:
		if (my_peer != 0)
			my_peer->b_mute.animateClick();
		break;
	default:
		break;
	}
}

void
HpsJamMixer :: handle_fader_level(uint8_t mix, uint8_t index, float left, float right)
{
	/* make scale logarithmic */
	left = level_encode(left);
	right = level_encode(right);

	switch (mix) {
	case 0:
		HPSJAM_NO_SIGNAL(peer_strip[index].w_slider,setLevel(left, right));
		break;
	case 255:
		HPSJAM_NO_SIGNAL(self_strip.w_slider,setLevel(left, right));
		break;
	default:
		break;
	}
}

void
HpsJamMixer :: handle_fader_self(uint8_t mix, uint8_t index)
{
	switch (mix) {
	case 0:
		my_peer = peer_strip + index;
		break;
	default:
		break;
	}
}

void
HpsJamMixer :: handle_fader_name(uint8_t mix, uint8_t index, QString *str)
{
	switch (mix) {
	case 0:
		HPSJAM_NO_SIGNAL(peer_strip[index].w_name,setText(*str));
		enable(index);
		break;
	case 255:
		HPSJAM_NO_SIGNAL(self_strip.w_name,setText(*str));
		break;
	default:
		break;
	}
	delete str;
}

void
HpsJamMixer :: handle_fader_icon(uint8_t mix, uint8_t index, QByteArray *ba)
{
	switch (mix) {
	case 0:
		peer_strip[index].w_icon.svg.load(*ba);
		peer_strip[index].w_icon.update();
		break;
	case 255:
		self_strip.w_icon.svg.load(*ba);
		self_strip.w_icon.update();
		break;
	default:
		break;
	}
	delete ba;
}
void
HpsJamMixer :: handle_fader_gain(uint8_t mix, uint8_t index, float gain)
{
	switch (mix) {
	case 0:
		HPSJAM_NO_SIGNAL(peer_strip[index].w_slider,setValue(gain));
		break;
	case 255:
		HPSJAM_NO_SIGNAL(self_strip.w_slider,setValue(gain));
		break;
	default:
		break;
	}
}

void
HpsJamMixer :: handle_fader_pan(uint8_t mix, uint8_t index, float pan)
{
	switch (mix) {
	case 0:
		HPSJAM_NO_SIGNAL(peer_strip[index].w_slider,setPan(pan));
		break;
	case 255:
		HPSJAM_NO_SIGNAL(self_strip.w_slider,setPan(pan));
		break;
	default:
		break;
	}
}

void
HpsJamMixer :: handle_fader_eq(uint8_t mix, uint8_t index, QString *str)
{
	switch (mix) {
	case 0:
		HPSJAM_NO_SIGNAL(peer_strip[index].w_eq.edit,setText(*str));
		break;
	case 255:
		HPSJAM_NO_SIGNAL(self_strip.w_eq.edit,setText(*str));
		break;
	default:
		break;
	}
	delete str;
}

void
HpsJamMixer :: handle_fader_disconnect(uint8_t mix, uint8_t index)
{
	switch (mix) {
	case 0:
		peer_strip[index].init();
		disable(index);
		break;
	case 255:
		self_strip.init();
		break;
	default:
		break;
	}
}

void
HpsJamMixer :: handle_bits_changed(int id)
{
	struct hpsjam_packet_entry *ptr;
	char bits;

	bits = peer_strip[id].getBits();

	ptr = new struct hpsjam_packet_entry;
	ptr->packet.type = HPSJAM_TYPE_FADER_BITS_REQUEST;
	ptr->packet.setFaderData(0, id, &bits, 1);

	hpsjam_client_peer->send_single_pkt(ptr);
}

void
HpsJamMixer :: handle_gain_changed(int id)
{
	struct hpsjam_packet_entry *ptr;
	float gain;

	gain = peer_strip[id].w_slider.value;

	ptr = new struct hpsjam_packet_entry;
	ptr->packet.type = HPSJAM_TYPE_FADER_GAIN_REQUEST;
	ptr->packet.setFaderValue(0, id, &gain, 1);

	hpsjam_client_peer->send_single_pkt(ptr);
}

void
HpsJamMixer :: handle_pan_changed(int id)
{
	struct hpsjam_packet_entry *ptr;
	float pan;

	pan = peer_strip[id].w_slider.pan;

	ptr = new struct hpsjam_packet_entry;
	ptr->packet.type = HPSJAM_TYPE_FADER_PAN_REQUEST;
	ptr->packet.setFaderValue(0, id, &pan, 1);

	hpsjam_client_peer->send_single_pkt(ptr);
}

void
HpsJamMixer :: handle_eq_changed(int id)
{
	struct hpsjam_packet_entry *ptr;
	QByteArray eq = peer_strip[id].w_eq.edit.toPlainText().toLatin1();

	if (eq.length() > 255)
		return;

	ptr = new struct hpsjam_packet_entry;
	ptr->packet.type = HPSJAM_TYPE_FADER_EQ_REQUEST;
	ptr->packet.setFaderData(0, id, eq.constData(), eq.length());

	hpsjam_client_peer->send_single_pkt(ptr);
}

void
HpsJamMixer :: handle_local_eq_changed()
{
	QByteArray eq = self_strip.w_eq.edit.toPlainText().toLatin1();

	QMutexLocker locker(&hpsjam_client_peer->lock);
	hpsjam_client_peer->local_eq.init(eq.constData());
}

void
HpsJamMixer :: enable(unsigned index)
{
	peer_strip[index].show();
	if (!hpsjam_client->w_mixer->isVisible())
		hpsjam_client->b_mixer.setFlashing();
}

void
HpsJamMixer :: disable(unsigned index)
{
	peer_strip[index].init();
	peer_strip[index].hide();
	if (!hpsjam_client->w_mixer->isVisible())
		hpsjam_client->b_mixer.setFlashing();
}

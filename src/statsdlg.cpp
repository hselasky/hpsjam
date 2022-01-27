/*-
 * Copyright (c) 2020-2021 Hans Petter Selasky. All rights reserved.
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

#include "peer.h"
#include "protocol.h"
#include "statsdlg.h"

#include <QMutexLocker>
#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QColor>

void
HpsJamStatsGraph :: paintEvent(QPaintEvent *event)
{
	constexpr unsigned N = 2 * HPSJAM_SEQ_MAX;
	uint64_t packet_loss;
	uint64_t packet_damage;
	uint16_t ping_time;
	uint16_t jitter_time;
	QRect frame(16, 16, width() - 32, height() - 32);
	float stats[N] = {};
	float fMax;
	int height = frame.height();
	int width = frame.width();
	int fsize = (height + 39) / 40;
	int xmax;

	/* get copy of live statistics */
	if (1) {
		QMutexLocker locker(&hpsjam_client_peer->lock);

		assert(sizeof(stats) >= sizeof(hpsjam_client_peer->input_pkt.jitter.stats));
		memcpy(stats, hpsjam_client_peer->input_pkt.jitter.stats, sizeof(stats));
		packet_loss = hpsjam_client_peer->input_pkt.jitter.packet_loss;
		packet_damage = hpsjam_client_peer->input_pkt.jitter.packet_damage;
		ping_time = hpsjam_client_peer->output_pkt.ping_time;
		jitter_time = hpsjam_client_peer->input_pkt.jitter.get_jitter_in_ms();
	}

	/* make room for text */
	height -= 2 * fsize;

	/* create painter */
	QPainter paint(this);

	/* select foreground color */
	QColor fg(0, 0, 0);

	/* select background color */
	QColor bg(255, 255, 255);

	paint.fillRect(frame, bg);

	l_status.setText(QString("%1 packets lost and %2 damaged. Round trip time is %3ms+%4ms")
	    .arg(packet_loss).arg(packet_damage).arg(ping_time).arg(jitter_time));

	for (unsigned i = xmax = 0; i != N; i++) {
		if (stats[xmax] < stats[i]) {
			xmax = i;
		}
	}
	fMax = stats[xmax];

	if (fMax > 1.0f) {
		for (unsigned i = 0; i != N; i++) {
			stats[i] /= fMax;
		}
	} else {
		memset(stats, 0, sizeof(stats));
	}

	float dX = (float)width / (float)N;

	QPoint lastPoint;

	for (unsigned i = 0; i != N; i++) {
		const QPoint nextPoint(
		    frame.x() + static_cast < int >(dX * i + dX / 2.0f),
		    frame.y() + 2 * fsize + height - 1 -
		    (int)(stats[i] * (height - fsize)));

		if (i == 0)
			lastPoint = nextPoint;

		paint.setPen(QPen(QBrush(fg), fsize, Qt::SolidLine, Qt::RoundCap));
		paint.drawPoint(nextPoint);

		paint.setPen(QPen(QBrush(fg), 2));
		paint.drawLine(lastPoint, nextPoint);

		lastPoint = nextPoint;
	}

	paint.setPen(QPen(QBrush(fg), 4));
	paint.drawRect(frame);
}

void
HpsJamStats :: handle_timer()
{
	w_graph.update();
}

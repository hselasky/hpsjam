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
	constexpr unsigned N = HPSJAM_MAX_JITTER;
	uint64_t packet_recover;
	uint64_t packet_damage;
	uint16_t ping_time;
	uint16_t jitter_time;
	uint16_t low_water[2];
	uint16_t high_water[2];
	int adjust[2];
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
		packet_recover = hpsjam_client_peer->input_pkt.jitter.packet_recover;
		packet_damage = hpsjam_client_peer->input_pkt.jitter.packet_damage;
		ping_time = hpsjam_client_peer->output_pkt.ping_time;
		jitter_time = hpsjam_client_peer->input_pkt.jitter.get_jitter_in_ms();
		low_water[0] = hpsjam_client_peer->in_audio[0].low_water;
		low_water[1] = hpsjam_client_peer->out_audio[0].low_water;
		high_water[0] = hpsjam_client_peer->in_audio[0].high_water;
		high_water[1] = hpsjam_client_peer->out_audio[0].high_water;
		adjust[0] = hpsjam_client_peer->in_audio[0].getWaterRef();
		adjust[1] = hpsjam_client_peer->out_audio[0].getWaterRef();
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

	l_status[0].setText(QString("Network :: %1 recovered and %2 damaged. Round trip time is %3ms+%4ms")
	    .arg(packet_recover).arg(packet_damage).arg(ping_time).arg(jitter_time));
	l_status[2].setText(QString("Local audio output :: Buffer level is %1 and %2, adjusting %3 samples, %4 ms jitter.")
	    .arg(low_water[0]).arg(high_water[0]).arg(adjust[0])
	    .arg((high_water[0] - low_water[0] + HPSJAM_DEF_SAMPLES - 1) / HPSJAM_DEF_SAMPLES));
	l_status[1].setText(QString("Local audio input :: Buffer level is %1 and %2, adjusting %3 samples, %4 ms jitter.")
	    .arg(low_water[1]).arg(high_water[1]).arg(adjust[1])
	    .arg((high_water[1] - low_water[1] + HPSJAM_DEF_SAMPLES - 1) / HPSJAM_DEF_SAMPLES));

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

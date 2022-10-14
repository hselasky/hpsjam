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

#ifndef _HPSJAM_STATSDLG_H_
#define	_HPSJAM_STATSDLG_H_

#include "texture.h"

#include <QObject>
#include <QWidget>
#include <QGridLayout>
#include <QTimer>
#include <QLabel>

class HpsJamStatsGraph : public QWidget {
public:
	QLabel l_status[3];
	void paintEvent(QPaintEvent *);
};

class HpsJamStats : public HpsJamTWidget {
	Q_OBJECT;
public:
	QGridLayout gl;

	HpsJamStatsGraph w_graph;

	HpsJamStats() : gl(this) {
		gl.addWidget(&w_graph.l_status[0], 0,0);
		gl.addWidget(&w_graph.l_status[1], 1,0);
		gl.addWidget(&w_graph.l_status[2], 2,0);
		gl.addWidget(&w_graph, 3,0);
		gl.setRowStretch(3,1);
		connect(&timer, SIGNAL(timeout()), this, SLOT(handle_timer()));
		timer.start(1000);
	};
	QTimer timer;

public slots:
	void handle_timer();
};

#endif		/* _HPSJAM_STATSDLG_H_ */

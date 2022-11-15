/*-
 * Copyright (c) 2022 Hans Petter Selasky. All rights reserved.
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

#ifndef	_HPSJAM_RECORDINGDLG_H_
#define	_HPSJAM_RECORDINGDLG_H_

#include "texture.h"

#include <QPlainTextEdit>
#include <QSlider>
#include <QTimer>

class HpsJamRecordingBox : public HpsJamGroupBox {
public:
	HpsJamRecordingBox() :
	    b_stop(tr("STOP")), b_start(tr("START")) {
		setTitle(tr("Audio recording"));

		gl.addWidget(&b_stop, 0,0);
		gl.addWidget(&b_start, 0,1);

		b_start.t.rgb = QColor(255,127,127,255);
		b_stop.t.rgb = QColor(255,127,127,255);
		b_stop.setFlat(true);
	};
	HpsJamPushButton b_stop;
	HpsJamPushButton b_start;
};

class HpsJamPlaybackBox : public HpsJamGroupBox {
public:
	HpsJamPlaybackBox() :
	    b_stop(tr("STOP")), b_start(tr("START")) {
		setTitle(tr("Audio playback"));

		gl.addWidget(&s_progress, 0,0,1,2);
		gl.addWidget(&b_stop, 1,0);
		gl.addWidget(&b_start, 1,1);

		b_stop.setFlat(true);

		s_progress.setRange(0, 999);
		s_progress.setOrientation(Qt::Horizontal);
		s_progress.setValue(0);
	};
	HpsJamPushButton b_stop;
	HpsJamPushButton b_start;
	QSlider s_progress;
};

class HpsJamFileListBox : public QPlainTextEdit {
	Q_OBJECT
public:
	HpsJamFileListBox() {
		setReadOnly(true);
		setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
		connect(this, SIGNAL(cursorPositionChanged()), this, SLOT(updateSelection()));
	};

	void mousePressEvent(QMouseEvent *event) {
		QPlainTextEdit::mousePressEvent(event);
		updateSelection();
	};

	void refresh();

public slots:
	void updateSelection(int = -1);

signals:
	void valueChanged(QString);
};

struct HpsJamRecordingBuffer {
	static constexpr int32_t limit = 0x7FFFFF00;
	size_t pull_add_samples(float *, float *, size_t);
	size_t pull_samples_s32_le(int32_t *, size_t);
	size_t push_samples(const float *, const float *, size_t);
	size_t push_samples_s32_le(const int32_t *, size_t);
	size_t pull_max() const;
	size_t push_max() const;
	void reset() {
		consumer_off = producer_off = 0;
		file = 0;
	};
	FILE *file;
	off_t filesize;
	float buffer[HPSJAM_SAMPLE_RATE * HPSJAM_CHANNELS];
	size_t consumer_off;
	size_t producer_off;
};

class HpsJamRecording : public HpsJamTWidget {
	Q_OBJECT
public:
	HpsJamRecording() : gl(this) {
		gl.addWidget(&playback, 0,0);
		gl.addWidget(&recording, 1,0);
		gl.addWidget(&filelist, 2,0);
		gl.setRowStretch(2,1);

		connect(&playback.b_stop, SIGNAL(pressed()), this, SLOT(handle_stop_playback()));
		connect(&playback.b_start, SIGNAL(pressed()), this, SLOT(handle_start_playback()));
		connect(&playback.s_progress, SIGNAL(valueChanged(int)), this, SLOT(handle_slider_value(int)));
		connect(&recording.b_stop, SIGNAL(pressed()), this, SLOT(handle_stop_record()));
		connect(&recording.b_start, SIGNAL(pressed()), this, SLOT(handle_start_record()));

		memset(&recbuf, 0, sizeof(recbuf));
		memset(&playbuf, 0, sizeof(playbuf));

		connect(&watchdog, SIGNAL(timeout()), this, SLOT(handle_watchdog()));

		watchdog.start(500);

		filelist.refresh();
	};
	QGridLayout gl;
	HpsJamPlaybackBox playback;
	HpsJamRecordingBox recording;
	HpsJamFileListBox filelist;

	QTimer watchdog;

	HpsJamRecordingBuffer recbuf;
	HpsJamRecordingBuffer playbuf;

	QString newFileName();
	FILE *open(const QString &, const char *, off_t &);

public slots:
	void handle_start_record();
	void handle_stop_record();
	void handle_start_playback();
	void handle_stop_playback();
	void handle_slider_value(int);
	void handle_watchdog();
};

#endif		/* _HPSJAM_RECORDINGDLG_H_ */

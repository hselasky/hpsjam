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

#ifndef _HPSJAM_CHATDLG_H_
#define	_HPSJAM_CHATDLG_H_

#include "texture.h"

#include <QObject>
#include <QWidget>
#include <QGroupBox>
#include <QGridLayout>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QPushButton>

class HpsJamChatLyrics : public HpsJamGroupBox {
	Q_OBJECT;
public:
	HpsJamChatLyrics() :
	    b_send(tr("Send a line of l&yrics")) {
		setTitle(tr("Send lyrics"));
#if defined(Q_OS_MACX)
		b_send.setShortcut(QKeySequence(Qt::ALT + Qt::Key_Y));
#endif
		edit.setTabChangesFocus(true);

		gl.addWidget(&edit, 0,0,1,2);
		gl.addWidget(&b_send, 1,1,1,1);
		gl.setRowStretch(0,1);
		gl.setColumnStretch(0,1);

		connect(&b_send, SIGNAL(released()), this, SLOT(handle_send_lyrics()));
	};
	HpsJamPushButton b_send;
	QPlainTextEdit edit;

public slots:
	void handle_send_lyrics();
};

class HpsJamChatBox : public HpsJamGroupBox {
	Q_OBJECT;
public:
	HpsJamChatBox() :
	    b_send(tr("SEND")), b_clear(tr("CLEA&R")) {
#if defined(Q_OS_MACX)
		b_clear.setShortcut(QKeySequence(Qt::ALT + Qt::Key_R));
#endif
		setTitle(tr("Chat box"));
		line.setMaxLength(128);
		edit.setReadOnly(true);
		edit.setMaximumBlockCount(1000);
		edit.setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);

		gl.addWidget(&edit, 0,0,1,3);
		gl.addWidget(&line, 1,0,1,1);
		gl.addWidget(&b_send, 1,1,1,1);
		gl.addWidget(&b_clear, 1,2,1,1);
		gl.setRowStretch(0,1);
		gl.setColumnStretch(0,1);

		connect(&b_send, SIGNAL(released()), this, SLOT(handle_send_chat()));
		connect(&b_clear, SIGNAL(released()), this, SLOT(handle_clear_chat()));
		connect(&line, SIGNAL(returnPressed()), this, SLOT(handle_send_chat()));
	};
	HpsJamPushButton b_send;
	HpsJamPushButton b_clear;
	QPlainTextEdit edit;
	QLineEdit line;

public slots:
	void handle_send_chat();
	void handle_clear_chat();
};

class HpsJamChat : public HpsJamTWidget {
public:
	HpsJamChat() : gl(this) {
		gl.addWidget(&lyrics, 0,0);
		gl.addWidget(&chat, 1,0);
		gl.setRowStretch(0,1);
		gl.setRowStretch(1,1);
	};
	void append(const QString &);
	QGridLayout gl;
	HpsJamChatLyrics lyrics;
	HpsJamChatBox chat;
};

#endif		/* _HPSJAM_CHATDLG_H_ */

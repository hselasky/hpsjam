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

#ifndef _HPSJAM_CONNECTDLG_H_
#define	_HPSJAM_CONNECTDLG_H_

#include "texture.h"

#include <QWidget>
#include <QGroupBox>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QGridLayout>
#include <QPushButton>
#include <QByteArray>
#include <QKeySequence>
#include <QComboBox>

class HpsJamConnectList : public QPlainTextEdit {
	Q_OBJECT
public:
	HpsJamConnectList() {
		setReadOnly(true);
		setMaximumBlockCount(HPSJAM_SERVER_LIST_MAX);
		setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);

		connect(this, SIGNAL(cursorPositionChanged()), this, SLOT(updateSelection()));
	};

	void mousePressEvent(QMouseEvent *event) {
		QPlainTextEdit::mousePressEvent(event);
		updateSelection();
	};
public slots:
	void updateSelection(const QString &);
	void updateSelection(int = -1);
signals:
	void valueChanged(const QString);
};

class HpsJamIcon;
class HpsJamConnectIcon : public HpsJamGroupBox {
	Q_OBJECT
public:
	enum { numIcons = HPSJAM_NUM_ICONS };

	HpsJamIcon *icon[numIcons];
	unsigned selection;

	HpsJamConnectIcon();
	void loadSelection(QByteArray &);
	void setEnabled(bool);
	void setSelection(unsigned);

public slots:
	void handle_selection();
};

class HpsJamConnectName : public HpsJamGroupBox {
	Q_OBJECT
public:
	HpsJamConnectName() {
		setTitle(tr("Select nickname"));
		edit.setText(QString("anonymous"));
		edit.setMaxLength(32);
		gl.addWidget(&edit, 0,0);
	};
	QLineEdit edit;
public slots:
	void setText(const QString other) {
		edit.setText(other);
	};
};

class HpsJamConnectPassword : public HpsJamGroupBox {
public:
	HpsJamConnectPassword() {
		setTitle(tr("Enter password, if any"));
		edit.setEchoMode(QLineEdit::Password);
		edit.setMaxLength(16);
		gl.addWidget(&edit, 0,0);
		setCollapsed(true);
	};
	QLineEdit edit;
};

class HpsJamConnectServer : public HpsJamGroupBox {
	Q_OBJECT
public:
	HpsJamConnectServer() {
		setTitle(tr("Enter server location"));
		edit.setText(QString("127.0.0.1" ":" HPSJAM_DEFAULT_PORT_STR));
		gl.addWidget(&edit, 0,0);
		gl.addWidget(&list, 1,0);
		gl.setRowStretch(1,1);
	};
	QLineEdit edit;
	HpsJamConnectList list;
public slots:
	void updateServer(const QString other) {
		edit.setText(other);
	};
};

class HpsJamConnectButtons : public QWidget {
public:
	HpsJamConnectButtons() :
            gl(this),
	    b_reset(tr("&Reset server list")),
	    b_connect(tr("C&onnect")),
	    b_disconnect(tr("&Disconnect")) {
		l_multi_port.addItem(QString("Multiport ON"));
		l_multi_port.addItem(QString("Multiport OFF"));

#if defined(Q_OS_MACX)
		b_reset.setShortcut(QKeySequence(Qt::ALT + Qt::Key_R));
		b_connect.setShortcut(QKeySequence(Qt::ALT + Qt::Key_O));
		b_disconnect.setShortcut(QKeySequence(Qt::ALT + Qt::Key_D));
#endif
		gl.addWidget(&b_reset, 0,0);
		gl.setColumnStretch(1,1);
		gl.addWidget(&l_multi_port, 0,2);
		gl.setColumnStretch(3,1);
		gl.addWidget(&b_connect, 0,4);
		gl.addWidget(&b_disconnect, 0,5);
	};
	QGridLayout gl;
	HpsJamPushButton b_reset;
	HpsJamPushButton b_connect;
	HpsJamPushButton b_disconnect;
	QComboBox l_multi_port;
};

class HpsJamConnect : public QWidget {
	Q_OBJECT
public:
	HpsJamConnect();

	QGridLayout gl;
	HpsJamConnectIcon icon;
	HpsJamConnectName name;
	HpsJamConnectPassword password;
	HpsJamConnectServer server;
	HpsJamConnectButtons buttons;

	void activate(bool state) {
		icon.setEnabled(state);
		buttons.b_connect.setEnabled(state);
		buttons.b_disconnect.setEnabled(!state);
		buttons.l_multi_port.setEnabled(state);
		name.edit.setEnabled(state);
		password.edit.setEnabled(state);
		server.edit.setEnabled(state);
		server.list.setEnabled(state);
	};
public slots:
	void handle_reset();
	void handle_connect();
	void handle_disconnect();
};

#endif		/* _HPSJAM_CONNECTDLG_H_ */

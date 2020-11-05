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

#ifndef _HPSJAM_CONNECTDLG_H_
#define	_HPSJAM_CONNECTDLG_H_

#include <QWidget>
#include <QGroupBox>
#include <QLineEdit>
#include <QListWidget>
#include <QGridLayout>
#include <QPushButton>

class HpsJamConnectList : public QListView {
	Q_OBJECT;
public:
	void selectionChanged(const QItemSelection &, const QItemSelection &);
signals:
	void valueChanged();
};

class HpsJamConnectIcon : public QGroupBox {
public:
	HpsJamConnectIcon() {
		setTitle(tr("Select icon"));
	};
};

class HpsJamConnectName : public QGroupBox {
public:
	HpsJamConnectName() : gl(this) {
		setTitle(tr("Select nickname"));
		gl.addWidget(&edit, 0,0);
	};
	QGridLayout gl;
	QLineEdit edit;
};

class HpsJamConnectPassword : public QGroupBox {
public:
	HpsJamConnectPassword() : gl(this) {
		setTitle(tr("Enter your password, if any"));
		edit.setEchoMode(QLineEdit::Password);
		gl.addWidget(&edit, 0,0);
	};
	QGridLayout gl;
	QLineEdit edit;
};

class HpsJamConnectLocation : public QGroupBox {
public:
	HpsJamConnectLocation() : gl(this) {
		setTitle(tr("Select location"));
		gl.addWidget(&list, 0,0);
	};
	QGridLayout gl;
	HpsJamConnectList list;
};

class HpsJamConnectServer : public QGroupBox {
public:
	HpsJamConnectServer() : gl(this) {
		setTitle(tr("Select server"));
		gl.addWidget(&edit, 0,0);
		gl.addWidget(&list, 1,0);
		gl.setRowStretch(1,1);
	};
	QGridLayout gl;
	QLineEdit edit;
	HpsJamConnectList list;
};

class HpsJamConnectButtons : public QWidget {
public:
	HpsJamConnectButtons() :
            gl(this),
	    b_refresh(tr("&Refresh")),
	    b_connect(tr("&Connect")),
	    b_disconnect(tr("&Disconnect")) {
		gl.addWidget(&b_refresh, 0,0);
		gl.setColumnStretch(1,1);
		gl.addWidget(&b_connect, 0,2);
		gl.addWidget(&b_disconnect, 0,3);
	};
	QGridLayout gl;
	QPushButton b_refresh;
	QPushButton b_connect;
	QPushButton b_disconnect;
};

class HpsJamConnect : public QWidget {
	Q_OBJECT;
public:
	HpsJamConnect();

	QGridLayout gl;
	HpsJamConnectIcon icon;
	HpsJamConnectName name;
	HpsJamConnectPassword password;
	HpsJamConnectLocation location;
	HpsJamConnectServer server;
	HpsJamConnectButtons buttons;

public slots:
	void handle_location_change();
	void handle_server_change();
	void handle_refresh();
	void handle_connect();
	void handle_disconnect();
};

#endif		/* _HPSJAM_CONNECTDLG_H_ */

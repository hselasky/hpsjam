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

#include <QMutexLocker>
#include <QMessageBox>
#include <QFile>

#include "hpsjam.h"
#include "peer.h"
#include "connectdlg.h"
#include "clientdlg.h"
#include "configdlg.h"
#include "mixerdlg.h"
#include "timer.h"

#if defined(Q_OS_MACX) || defined(Q_OS_IOS)
#include <activity.h>
#endif

void
HpsJamConnectList :: updateSelection(const QString &other)
{
	QString temp = toPlainText();
	QStringList list = temp.split(QString("\n"));

	for (int x = 0; x != list.length(); ) {
		if (list[x].isEmpty())
			list.removeAt(x);
		else
			x++;
	}

	for (int x = 0; x != list.length(); x++) {
		if (list[x] == other) {
			updateSelection(x);
			return;
		} else if (list[x] > other) {
			if (list.length() >= HPSJAM_SERVER_LIST_MAX)
				return;
			list.insert(x, other);
			setPlainText(list.join("\n"));
			updateSelection(x);
			return;
		}
	}
	if (list.length() >= HPSJAM_SERVER_LIST_MAX)
		return;
	list.append(other);
	setPlainText(list.join("\n"));
	updateSelection(list.length() - 1);
}

void
HpsJamConnectList :: updateSelection(int which)
{
	QTextCursor c = textCursor();
	int old = c.blockNumber();

	if (which > -1) {
		c.movePosition(QTextCursor::Start, QTextCursor::MoveAnchor);
		c.movePosition(QTextCursor::NextBlock, QTextCursor::MoveAnchor, which);
	}
	c.movePosition(QTextCursor::StartOfLine, QTextCursor::MoveAnchor);
	c.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);

	if (which == -1 || old != c.blockNumber()) {
		QString s = c.selectedText();
		if (!s.isEmpty())
			emit valueChanged(s);
	}
	setTextCursor(c);
}

HpsJamConnectIcon :: HpsJamConnectIcon()
{
	selection = 0;

	icon[0] = new HpsJamIcon(QString());
	icon[1] = new HpsJamIcon(QString(":/icons/heart.svg"));
	icon[2] = new HpsJamIcon(QString(":/icons/bell.svg"));
	icon[3] = new HpsJamIcon(QString(":/icons/circle.svg"));
	icon[4] = new HpsJamIcon(QString(":/icons/speaker.svg"));
	icon[5] = new HpsJamIcon(QString(":/icons/smiley.svg"));
	icon[6] = new HpsJamIcon(QString(":/icons/guitar.svg"));
	icon[7] = new HpsJamIcon(QString(":/icons/microphone.svg"));
	icon[8] = new HpsJamIcon(QString(":/icons/tuba.svg"));
	icon[9] = new HpsJamIcon(QString(":/icons/flute.svg"));
	icon[10] = new HpsJamIcon(QString(":/icons/waveform.svg"));
	icon[11] = new HpsJamIcon(QString(":/icons/sun.svg"));
	icon[12] = new HpsJamIcon(QString(":/icons/jack.svg"));
	icon[13] = new HpsJamIcon(QString(":/icons/note.svg"));

	for (unsigned x = 0; x != numIcons; x++) {
		HPSJAM_NO_SIGNAL(icon[x][0],setSelection(x == selection));
		gl.addWidget(icon[x], x / 7, x % 7);
		connect(icon[x], SIGNAL(selected()), this, SLOT(handle_selection()));
	}

	setTitle(tr("Select icon"));
	setCollapsed(true);
}

void
HpsJamConnectIcon :: setSelection(unsigned _selection)
{
	if (_selection == selection || _selection >= HPSJAM_NUM_ICONS)
		return;
	selection = _selection;

	for (unsigned x = 0; x != numIcons; x++)
		HPSJAM_NO_SIGNAL(icon[x][0],setSelection(x == selection));

	update();
}

void
HpsJamConnectIcon :: setEnabled(bool state)
{
	for (unsigned x = 0; x != numIcons; x++)
		icon[x]->setEnabled(state);
}

void
HpsJamConnectIcon :: loadSelection(QByteArray &ba)
{
	const QString &fname = icon[selection]->fname;

	if (fname.isEmpty()) {
		ba = QByteArray();
	} else {
		QFile file(fname);

		if (file.open(QIODevice::ReadOnly))
			ba = file.readAll();
		else
			ba = QByteArray();
	}
}

void
HpsJamConnectIcon :: handle_selection()
{
	for (unsigned x = 0; x != numIcons; x++) {
		if (sender() == icon[x]) {
			HPSJAM_NO_SIGNAL(icon[x][0],setSelection(true));
			selection = x;
		} else {
			HPSJAM_NO_SIGNAL(icon[x][0],setSelection(false));
		}
	}
}

HpsJamConnect :: HpsJamConnect() : gl(this)
{
	gl.addWidget(&icon, 0,0);
	gl.addWidget(&name, 1,0);
	gl.addWidget(&password, 2,0);
	gl.addWidget(&server, 3,0);
	gl.setRowStretch(4,1);
	gl.addWidget(&buttons, 5,0);

	connect(&server.list, SIGNAL(valueChanged(const QString)), &server.edit, SLOT(setText(const QString)));
	connect(&buttons.b_reset, SIGNAL(released()), this, SLOT(handle_reset()));
	connect(&buttons.b_connect, SIGNAL(released()), this, SLOT(handle_connect()));
	connect(&buttons.b_disconnect, SIGNAL(released()), this, SLOT(handle_disconnect()));

	connect(&hpsjam_client_peer->output_pkt, SIGNAL(pendingTimeout()), this, SLOT(handle_disconnect()));

	buttons.b_disconnect.setEnabled(false);
}

void
HpsJamConnect :: handle_reset()
{
	server.list.setPlainText(QString());
}

void
HpsJamConnect :: handle_connect()
{
	QString nick = name.edit.text().trimmed();
	QString text = server.edit.text().trimmed();
	QString passwd = password.edit.text().trimmed();
	struct hpsjam_socket_address address;
	struct hpsjam_packet_entry *pkt;
	unsigned long long key = 0;
	QByteArray host;
	QByteArray port;
	QByteArray temp;
	QByteArray idata;
	bool multiPort = (buttons.l_multi_port.currentIndex() == 0);

	auto parts = text.split(QString(":"));

	switch (parts.length()) {
	case 1:
		host = parts[0].toLatin1();
		port = HPSJAM_DEFAULT_PORT_STR;
		break;
	case 2:
		host = parts[0].toLatin1();
		port = parts[1].toLatin1();
		break;
	default:
		QMessageBox::information(this, tr("CONNECT"),
		    tr("Invalid server name: %1").arg(text));
		return;
	}

	if (nick.isEmpty()) {
		QMessageBox::information(this, tr("CONNECT"),
		    tr("Invalid nick name: %1").arg(nick));
		return;
	}

	if (!passwd.isEmpty()) {
		QByteArray temp = passwd.toLatin1();
		if (::sscanf(temp.constData(), "%llx", &key) != 1) {
			QMessageBox::information(this, tr("CONNECT"),
			    tr("Invalid password name: %1").arg(passwd));
			return;
		}
	}

	icon.loadSelection(idata);

	if (idata.length() > 1000) {
		QMessageBox::information(this, tr("CONNECT"),
		    tr("Icon size is bigger than 1000 bytes.\n"
		       "Try selecting another icon."));
		return;
	}

	if (hpsjam_v4[0].resolve(host.constData(), port.constData(), address) == false) {
		if (hpsjam_v6[0].resolve(host.constData(), port.constData(), address) == false) {
			QMessageBox::information(this, tr("CONNECT"),
			    tr("Could not resolve server at: %1").arg(text));
			return;
		}
	}

	server.list.updateSelection(text);

	activate(false);

	QMutexLocker locker(&hpsjam_client_peer->lock);

	/* set destination address */
	for (unsigned i = 0; i != HPSJAM_PORTS_MAX; i++) {
		hpsjam_client_peer->address[i] = address;
		switch (address.v4.sin_family) {
		case AF_INET:
			address.v4.sin_port =
			    htons(ntohs(address.v4.sin_port) + 1);
			break;
		case AF_INET6:
			address.v6.sin6_port =
			    htons(ntohs(address.v6.sin6_port) + 1);
			break;
		default:
			break;
		}
	}

	/* send initial ping */
	pkt = new struct hpsjam_packet_entry;
	pkt->packet.setPing(0, hpsjam_ticks, key,
	    multiPort ? HPSJAM_FEATURE_MULTI_PORT : 0);
	pkt->packet.type = HPSJAM_TYPE_PING_REQUEST;
	pkt->insert_tail(&hpsjam_client_peer->output_pkt.head);

	/* send initial configuration */
	pkt = new struct hpsjam_packet_entry;
	pkt->packet.setConfigure(hpsjam_client->w_config->down_fmt.format);
	pkt->packet.type = HPSJAM_TYPE_CONFIGURE_REQUEST;
	pkt->insert_tail(&hpsjam_client_peer->output_pkt.head);

	/* send name */
	temp = nick.toUtf8();
	pkt = new struct hpsjam_packet_entry;
	pkt->packet.setRawData(temp.constData(), temp.length());
	pkt->packet.type = HPSJAM_TYPE_NAME_REQUEST;
	pkt->insert_tail(&hpsjam_client_peer->output_pkt.head);

	/* send icon */
	pkt = new struct hpsjam_packet_entry;
	pkt->packet.setRawData(idata.constData(), idata.length(), ' ');
	pkt->packet.type = HPSJAM_TYPE_ICON_REQUEST;
	pkt->insert_tail(&hpsjam_client_peer->output_pkt.head);

	/* set local format, nickname and icon */
	hpsjam_client_peer->output_fmt = hpsjam_client->w_config->up_fmt.format;
	hpsjam_client->w_mixer->self_strip.w_name.setText(nick);
	hpsjam_client->w_mixer->self_strip.w_icon.svg.load(idata);
	hpsjam_client->w_mixer->self_strip.w_icon.update();
	hpsjam_client->setWindowTitle(
	    QString(HPSJAM_WINDOW_TITLE " Client " HPSJAM_VERSION_STRING) + QString(" - ") + nick);

	/* save the settings */
	hpsjam_client->saveSettings();

#if defined(Q_OS_MACX) || defined(Q_OS_IOS)
	HpsJamBeginActivity();
#endif
}

void
HpsJamConnect :: handle_disconnect()
{
	/* ignore double events */
	if (buttons.b_connect.isEnabled())
		return;

	activate(true);

	QMutexLocker locker(&hpsjam_client_peer->lock);
	hpsjam_client_peer->init();
	hpsjam_default_midi[0].clear();
	locker.unlock();

	hpsjam_client->w_mixer->init();

#if defined(Q_OS_MACX) || defined(Q_OS_IOS)
	HpsJamEndActivity();
#endif
	if (!isVisible()) {
		QMessageBox::information(this, tr("DISCONNECTED"),
		    tr("You were disconnected from the server!"));
	}
}

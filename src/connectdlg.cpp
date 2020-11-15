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
#include <QMessageBox>
#include <QFile>

#include "hpsjam.h"
#include "peer.h"
#include "connectdlg.h"
#include "clientdlg.h"
#include "configdlg.h"
#include "mixerdlg.h"
#include "timer.h"

HpsJamConnectIcon :: HpsJamConnectIcon() : gl(this)
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
		icon[x]->setSelection(x == selection);
		gl.addWidget(icon[x], x / 7, x % 7);
		connect(icon[x], SIGNAL(selected()), this, SLOT(handle_selection()));
	}

	setTitle(tr("Select icon"));
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
	QFile file;

	if (icon[selection]->fname.isEmpty())
		goto error;

	file.setFileName(icon[selection]->fname);

	if(!file.open(QIODevice::ReadOnly))
		goto error;
	ba = file.readAll();
	return;
error:
	ba = QByteArray();
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

void
HpsJamConnectList :: selectionChanged(const QItemSelection &cur, const QItemSelection &prev)
{
	QListView::selectionChanged(cur, prev);
	emit valueChanged();
}

HpsJamConnect :: HpsJamConnect() : gl(this)
{
	gl.addWidget(&icon, 0,0);
	gl.addWidget(&name, 1,0);
	gl.addWidget(&password, 2,0);
	gl.addWidget(&server, 3,0);
	gl.setRowStretch(3,1);
	gl.addWidget(&buttons, 4,0);

	connect(&server.list, SIGNAL(valueChanged()), this, SLOT(handle_server_change()));
	connect(&buttons.b_refresh, SIGNAL(released()), this, SLOT(handle_refresh()));
	connect(&buttons.b_connect, SIGNAL(released()), this, SLOT(handle_connect()));
	connect(&buttons.b_disconnect, SIGNAL(released()), this, SLOT(handle_disconnect()));

	connect(&hpsjam_client_peer->output_pkt, SIGNAL(pendingTimeout()), this, SLOT(handle_disconnect()));

	buttons.b_disconnect.setEnabled(false);
}

void
HpsJamConnect :: handle_server_change()
{

}

void
HpsJamConnect :: handle_refresh()
{

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

	auto parts = text.split(QString(":"));

	switch (parts.length()) {
	case 1:
		host = parts[0].toLatin1();
		port = HPSJAM_DEFAULT_IPV4_PORT_STR;
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

	if (hpsjam_v4.resolve(host.constData(), port.constData(), address) == false) {
		if (hpsjam_v6.resolve(host.constData(), port.constData(), address) == false) {
			QMessageBox::information(this, tr("CONNECT"),
			    tr("Could not resolve server at: %1").arg(text));
			return;
		}
	}

	activate(false);

	QMutexLocker locker(&hpsjam_client_peer->lock);

	/* set destination address */
	hpsjam_client_peer->address = address;

	/* send initial ping */
	pkt = new struct hpsjam_packet_entry;
	pkt->packet.setPing(0, hpsjam_ticks, key);
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
}

void
HpsJamConnect :: handle_disconnect()
{
	/* ignore double events */
	if (buttons.b_connect.isEnabled())
		return;

	activate(true);

	if (1) {
		QMutexLocker locker(&hpsjam_client_peer->lock);
		hpsjam_client_peer->init();
	}

	hpsjam_client->w_mixer->init();

	if (!isVisible()) {
		QMessageBox::information(this, tr("DISCONNECTED"),
		    tr("You were disconnected from the server!"));
	}
}

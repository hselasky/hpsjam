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
#include <QTextCursor>

#include "hpsjam.h"
#include "clientdlg.h"
#include "chatdlg.h"
#include "peer.h"

void
HpsJamChatLyrics :: handle_send_lyrics()
{
	QTextCursor cursor(edit.textCursor());

	cursor.beginEditBlock();
	cursor.movePosition(QTextCursor::Start, QTextCursor::MoveAnchor, 1);
	cursor.movePosition(QTextCursor::StartOfLine, QTextCursor::MoveAnchor, 1);
	cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor, 1);
	cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 1);
	QString str = cursor.selectedText();
	str.truncate(128);
	QByteArray temp = str.toUtf8();
	cursor.removeSelectedText();
	cursor.endEditBlock();

	QMutexLocker locker(&hpsjam_client_peer->lock);
	struct hpsjam_packet_entry *pkt;

	/* send text */
	pkt = new struct hpsjam_packet_entry;
	pkt->packet.setRawData(temp.constData(), temp.length());
	pkt->packet.type = HPSJAM_TYPE_LYRICS_REQUEST;
	pkt->insert_tail(&hpsjam_client_peer->output_pkt.head);
}

void
HpsJamChatBox :: handle_send_chat()
{
	QByteArray temp = line.text().toUtf8();
	struct hpsjam_packet_entry *pkt;

	line.setText(QString());

	QMutexLocker locker(&hpsjam_client_peer->lock);

	/* send text */
	pkt = new struct hpsjam_packet_entry;
	pkt->packet.setRawData(temp.constData(), temp.length());
	pkt->packet.type = HPSJAM_TYPE_CHAT_REQUEST;
	pkt->insert_tail(&hpsjam_client_peer->output_pkt.head);
}

void
HpsJamChat :: append(const QString &str)
{
	chat.edit.appendPlainText(str);

	if (!isVisible())
		hpsjam_client->b_chat.setFlashing();
}

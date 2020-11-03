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

#include "connectdlg.h"

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
	gl.addWidget(&location, 3,0);
	gl.addWidget(&server, 4,0);
	gl.setRowStretch(4,1);
	gl.addWidget(&buttons, 5,0);

	connect(&location.list, SIGNAL(valueChanged()), this, SLOT(handle_location_change()));
	connect(&server.list, SIGNAL(valueChanged()), this, SLOT(handle_server_change()));
	connect(&buttons.b_refresh, SIGNAL(released()), this, SLOT(handle_refresh()));
	connect(&buttons.b_connect, SIGNAL(released()), this, SLOT(handle_connect()));
	connect(&buttons.b_disconnect, SIGNAL(released()), this, SLOT(handle_disconnect()));

	buttons.b_disconnect.setEnabled(false);
}

void
HpsJamConnect :: handle_location_change()
{

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
	buttons.b_connect.setEnabled(false);
	buttons.b_disconnect.setEnabled(true);
}

void
HpsJamConnect :: handle_disconnect()
{
	buttons.b_connect.setEnabled(true);
	buttons.b_disconnect.setEnabled(false);
}

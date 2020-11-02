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

#ifndef	_HPSJAM_PEER_H_
#define	_HPSJAM_PEER_H_

#include <QString>
#include <QByteArray>

#include "audiobuffer.h"
#include "socket.h"
#include "protocol.h"

#include <stdbool.h>

class hpsjam_server_peer {
public:
	struct hpsjam_socket_address address;
	struct hpsjam_input_packetizer input_pkt;
	struct hpsjam_output_packetizer output_pkt;
	class hpsjam_audio_buffer in_audio[2];
	class hpsjam_audio_buffer out_audio[2];

	QString name;
	QByteArray icon;
	uint8_t bits[256];
#define	HPSJAM_BIT_MUTE (1 << 0)
#define	HPSJAM_BIT_SOLO (1 << 1)
#define	HPSJAM_BIT_INVERT (1 << 2)
	float gain;
	uint8_t input_fmt;
	uint8_t output_fmt;
	bool valid;

	void init() {
		input_pkt.init();
		output_pkt.init();
		in_audio[0].clear();
		in_audio[1].clear();
		out_audio[0].clear();
		out_audio[1].clear();
		name = QString();
		icon = QByteArray();
		input_fmt = 0;
		output_fmt = 0;
		gain = 1.0f;
		valid = false;
	};

	hpsjam_server_peer() {
		init();
	};
};

class hpsjam_client_peer {
public:
	struct hpsjam_socket_address address;
	struct hpsjam_input_packetizer input_pkt;
	struct hpsjam_output_packetizer output_pkt;
	class hpsjam_audio_buffer in_audio[2];
	class hpsjam_audio_buffer out_audio[2];
	bool valid[256];

	void init() {
		memset(valid, 0, sizeof(valid));
		input_pkt.init();
		output_pkt.init();
		in_audio[0].clear();
		in_audio[1].clear();
		out_audio[0].clear();
		out_audio[1].clear();
	};
	hpsjam_client_peer() {
		init();
	};
	void sound_process(float *, float *, size_t);
};

extern void hpsjam_peer_receive(const struct hpsjam_socket_address &,
    const union hpsjam_frame &);

#endif		/* _HPSJAM_PEER_H_ */

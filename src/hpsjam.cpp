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

#include "hpsjam.h"
#include "peer.h"
#include "clientdlg.h"
#include "connectdlg.h"
#include "configdlg.h"
#include "timer.h"

#include "../mac/activity.h"

#include <QApplication>
#include <QCoreApplication>
#include <QMessageBox>

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <err.h>

unsigned hpsjam_num_server_peers;
unsigned hpsjam_udp_buffer_size;
uint64_t hpsjam_server_passwd;
uint64_t hpsjam_mixer_passwd;
class hpsjam_server_peer *hpsjam_server_peers;
class hpsjam_client_peer *hpsjam_client_peer;
class HpsJamClient *hpsjam_client;
struct hpsjam_socket_address hpsjam_v4;
struct hpsjam_socket_address hpsjam_v6;
struct hpsjam_socket_address hpsjam_cli;
const char *hpsjam_welcome_message_file;

static const struct option hpsjam_opts[] = {
	{ "NSDocumentRevisionsDebugMode", required_argument, NULL, ' ' },
	{ "port", required_argument, NULL, 'p' },
	{ "ipv4-port", required_argument, NULL, 't' },
	{ "ipv6-port", required_argument, NULL, 'u' },
	{ "cli-port", required_argument, NULL, 'q' },
	{ "welcome-msg-file", required_argument, NULL, 'w' },
	{ "server", no_argument, NULL, 's' },
	{ "peers", required_argument, NULL, 'P' },
	{ "password", required_argument, NULL, 'K' },
	{ "mixer-password", required_argument, NULL, 'M' },
#ifndef _WIN32
	{ "daemon", no_argument, NULL, 'B' },
#endif
	{ "nickname", required_argument, NULL, 'N'},
	{ "icon", required_argument, NULL, 'i'},
	{ "connect", required_argument, NULL, 'c'},
	{ "audio-uplink-format", required_argument, NULL, 'U'},
	{ "audio-downlink-format", required_argument, NULL, 'D'},
	{ "audio-input-device", required_argument, NULL, 'I'},
	{ "audio-output-device", required_argument, NULL, 'O'},
	{ "audio-input-left", required_argument, NULL, 'l'},
	{ "audio-output-left", required_argument, NULL, 'L'},
	{ "audio-input-right", required_argument, NULL, 'r'},
	{ "audio-output-right", required_argument, NULL, 'R'},
#ifdef HAVE_JACK_AUDIO
	{ "jacknoconnect", no_argument, NULL, 'J' },
	{ "jackname", required_argument, NULL, 'n' },
#endif
	{ NULL, 0, NULL, 0 }
};

static void
usage(void)
{
        fprintf(stderr, "HpsJam [--server --peers <1..256>] [--port " HPSJAM_DEFAULT_IPV4_PORT_STR "] "
#ifndef _WIN32
		"[--daemon] \\\n"
#endif
		"	[--password <64_bit_hexadecimal_password>] \\\n"
#ifdef HAVE_JACK_AUDIO
		"	[--jacknoconnect] [--jackname <name>] \\\n"
#endif
		"	[--nickname <nickname>] \\\n"
		"	[--icon <0..%u>] \\\n"
		"	[--connect <servername:port>] \\\n"
		"	[--audio-uplink-format <0..%u>] \\\n"
		"	[--audio-downlink-format <0..%u>] \\\n"

		"	[--audio-input-device <0,1,2,3 ... , Default is 0>] \\\n"
		"	[--audio-output-device <0,1,2,3 ... , Default is 0>] \\\n"
		"	[--audio-input-left <0,1,2,3 ... , Default is 0>] \\\n"
		"	[--audio-output-left <0,1,2,3 ... , Default is 0>] \\\n"
		"	[--audio-input-right <0,1,2,3 ... , Default is 1>] \\\n"
		"	[--audio-output-right <0,1,2,3 ... , Default is 1>] \\\n"
		"	[--ipv4-port " HPSJAM_DEFAULT_IPV4_PORT_STR "] \\\n"
		"	[--ipv6-port " HPSJAM_DEFAULT_IPV6_PORT_STR "] \\\n"
		"	[--mixer-password <64_bit_hexadecimal_password>] \\\n"
		"	[--welcome-msg-file <filename> \\\n"
		"	[--cli-port <portnumber>]\n",
		HPSJAM_NUM_ICONS - 1,
		HPSJAM_AUDIO_FORMAT_MAX - 1,
		HPSJAM_AUDIO_FORMAT_MAX - 1);
        exit(1);
}

Q_DECL_EXPORT int
main(int argc, char **argv)
{
	static const char hpsjam_short_opts[] = {
	    "M:q:p:sP:hBJ:n:K:t:u:w:N:i:c:U:D:I:O:l:L:r:R:"
	};
	int c;
	int ipv4_port = HPSJAM_DEFAULT_IPV4_PORT;
	int ipv6_port = HPSJAM_DEFAULT_IPV6_PORT;
	int cliport = 0;
#ifndef _WIN32
	int do_fork = 0;
#endif
	bool jackconnect = true;
	const char *jackname = "hpsjam";
	const char *nickname = 0;
	const char *passwd = 0;
	const char *connect_to = 0;
	int icon_nr = -1;
	int uplink_format = -1;
	int downlink_format = -1;
	int input_device = -1;
	int output_device = -1;
	int input_left = -1;
	int output_left = -1;
	int input_right = -1;
	int output_right = -1;

	while ((c = getopt_long_only(argc, argv, hpsjam_short_opts, hpsjam_opts, NULL)) != -1) {
		switch (c) {
		case 'w':
			hpsjam_welcome_message_file = optarg;
			break;
		case 's':
			if (hpsjam_num_server_peers == 0)
				hpsjam_num_server_peers = 1;
			break;
		case 'p':
			ipv4_port = ipv6_port = atoi(optarg);
			if (ipv4_port <= 0 || ipv4_port >= 65536)
				usage();
			break;
		case 't':
			ipv4_port = atoi(optarg);
			if (ipv4_port <= 0 || ipv4_port >= 65536)
				usage();
			break;
		case 'u':
			ipv6_port = atoi(optarg);
			if (ipv6_port <= 0 || ipv6_port >= 65536)
				usage();
			break;
		case 'q':
			cliport = atoi(optarg);
			if (cliport <= 0 || cliport >= 65536)
				usage();
			break;
		case 'P':
			if (hpsjam_num_server_peers == 0)
				usage();
			hpsjam_num_server_peers = atoi(optarg);
			if (hpsjam_num_server_peers == 0 || hpsjam_num_server_peers > HPSJAM_PEERS_MAX)
				usage();
			break;
		case 'U':
			uplink_format = atoi(optarg);
			if (uplink_format < 0 || uplink_format > HPSJAM_AUDIO_FORMAT_MAX - 1)
				usage();
			break;
		case 'D':
			downlink_format = atoi(optarg);
			if (downlink_format < 0 || downlink_format > HPSJAM_AUDIO_FORMAT_MAX - 1)
				usage();
			break;
		case 'I':
			input_device = atoi(optarg);
			if (input_device < 0)
				usage();
			break;
		case 'O':
			output_device = atoi(optarg);
			if (output_device < 0)
				usage();
			break;
		case 'l':
			input_left = atoi(optarg);
			if (input_left < 0)
				usage();
			break;
		case 'L':
			output_left = atoi(optarg);
			if (output_left < 0)
				usage();
			break;
		case 'r':
			input_right = atoi(optarg);
			if (input_right < 0)
				usage();
			break;
		case 'R':
			output_right = atoi(optarg);
			if (output_right < 0)
				usage();
			break;
#ifndef _WIN32
		case 'B':
			do_fork = 1;
			break;
#endif
		case 'J':
			jackconnect = false;
			break;
		case 'n':
			jackname = optarg;
			break;
		case 'K':
			passwd = optarg;

			if (sscanf(optarg, "%llx", (long long *)&hpsjam_server_passwd) != 1)
				usage();
			break;
		case 'M':
			if (sscanf(optarg, "%llx", (long long *)&hpsjam_mixer_passwd) != 1)
				usage();
			break;
		case 'N':
			nickname = optarg;
			break;
		case 'i':
			icon_nr = atoi(optarg);
			if (icon_nr < 0 || icon_nr > HPSJAM_NUM_ICONS - 1)
				usage();
			break;
		case 'c':
			connect_to = optarg;
			break;
		case ' ':
			/* ignore */
			break;
		default:
			usage();
			break;
		}
	}

#ifndef _WIN32
	if (do_fork && daemon(0, 0) != 0)
		errx(1, "Cannot daemonize");
#endif

	qRegisterMetaType<uint8_t>("uint8_t");

	if (hpsjam_num_server_peers == 0) {
		QApplication app(argc, argv);

		/* set consistent double click interval */
		app.setDoubleClickInterval(250);

		hpsjam_client_peer = new class hpsjam_client_peer;
		hpsjam_client = new HpsJamClient();

		if (input_device < 0)
			input_device = hpsjam_client->input_device;
		if (output_device < 0)
			output_device = hpsjam_client->output_device;
		if (input_left < 0)
			input_left = hpsjam_client->input_left;
		if (output_left < 0)
			output_left = hpsjam_client->output_left;
		if (input_right < 0)
			input_right = hpsjam_client->input_right;
		if (output_right < 0)
			output_right = hpsjam_client->output_right;

#ifdef HAVE_JACK_AUDIO
		if (hpsjam_sound_init(jackname, jackconnect)) {
			QMessageBox::information(hpsjam_client, QObject::tr("NO AUDIO"),
				QObject::tr("Cannot connect to JACK server or \n"
					    "sample rate is different from %1Hz or \n"
					    "latency is too high").arg(HPSJAM_SAMPLE_RATE));
		}
		/* register exit hook for audio */
		atexit(&hpsjam_sound_uninit);
#endif

#ifdef HAVE_MAC_AUDIO
		if (hpsjam_sound_init(0, 0)) {
			QMessageBox::information(hpsjam_client, QObject::tr("NO AUDIO"),
				QObject::tr("Cannot connect to audio subsystem.\n"
					    "Check that you have an audio device connected and\n"
					    "that the sample rate is set to %1Hz.").arg(HPSJAM_SAMPLE_RATE));
		}
		QString status;
		hpsjam_sound_get_input_status(status);
		hpsjam_client->w_config->audio_dev.l_input.setText(status);
		hpsjam_sound_get_output_status(status);
		hpsjam_client->w_config->audio_dev.l_output.setText(status);

		if (input_device > -1 && hpsjam_client->w_config->audio_dev.handle_toggle_input_device(input_device) < 0) {
			QMessageBox::information(hpsjam_client, QObject::tr("NO AUDIO"),
				QObject::tr("Cannot find the specified audio input device"));
		}
		if (output_device > -1 && hpsjam_client->w_config->audio_dev.handle_toggle_output_device(output_device) < 0) {
			QMessageBox::information(hpsjam_client, QObject::tr("NO AUDIO"),
				QObject::tr("Cannot find the specified audio output device"));
		}
		if (output_left > -1)
			hpsjam_client->w_config->audio_dev.handle_toggle_output_left(output_left);
		if (output_right > -1)
			hpsjam_client->w_config->audio_dev.handle_toggle_output_right(output_right);
		if (input_left > -1)
			hpsjam_client->w_config->audio_dev.handle_toggle_input_left(input_left);
		if (input_right > -1)
			hpsjam_client->w_config->audio_dev.handle_toggle_input_right(input_right);

		/* register exit hook for audio */
		atexit(&hpsjam_sound_uninit);
#endif

#ifdef HAVE_ASIO_AUDIO
		if (hpsjam_sound_init(0, 0)) {
			QMessageBox::information(hpsjam_client, QObject::tr("NO AUDIO"),
				    QObject::tr("Cannot connect to ASIO subsystem or \n"
						"sample rate is different from %1Hz or \n"
						"buffer size is different from 96 samples.").arg(HPSJAM_SAMPLE_RATE));
		}
		QString status;
		hpsjam_sound_get_input_status(status);
		hpsjam_client->w_config->audio_dev.l_input.setText(status);
		hpsjam_sound_get_output_status(status);
		hpsjam_client->w_config->audio_dev.l_output.setText(status);

		if (input_device > -1 && hpsjam_client->w_config->audio_dev.handle_toggle_input_device(input_device) < 0) {
			QMessageBox::information(hpsjam_client, QObject::tr("NO AUDIO"),
				QObject::tr("Cannot find the specified audio device"));
		}
		if (output_left > -1)
			hpsjam_client->w_config->audio_dev.handle_toggle_output_left(output_left);
		if (output_right > -1)
			hpsjam_client->w_config->audio_dev.handle_toggle_output_right(output_right);
		if (input_left > -1)
			hpsjam_client->w_config->audio_dev.handle_toggle_input_left(input_left);
		if (input_right > -1)
			hpsjam_client->w_config->audio_dev.handle_toggle_input_right(input_right);

		/* register exit hook for audio */
		atexit(&hpsjam_sound_uninit);
#endif

		/* use settings passed from the command line, if any */
		if (nickname != 0)
			hpsjam_client->w_connect->name.edit.setText(QString::fromUtf8(nickname));
		if (icon_nr > -1)
			hpsjam_client->w_connect->icon.selection = icon_nr;
		if (passwd != 0)
			hpsjam_client->w_connect->password.edit.setText(QString::fromUtf8(passwd));
		if (connect_to != 0)
			hpsjam_client->w_connect->server.edit.setText(QString::fromUtf8(connect_to));
		if (uplink_format > -1)
			hpsjam_client->w_config->up_fmt.setIndex(uplink_format);
		if (downlink_format > -1)
			hpsjam_client->w_config->down_fmt.setIndex(downlink_format);

		/* set a valid UDP buffer size */
		hpsjam_udp_buffer_size = 2000 * HPSJAM_SEQ_MAX;

		/* create sockets, if any */
		hpsjam_socket_init(ipv4_port, ipv6_port, cliport);

		/* create timer, if any */
		hpsjam_timer_init();

		if (connect_to != 0) {
			/* wait a bit for sockets and timer to be created */
			usleep(250000);
			hpsjam_client->w_connect->handle_connect();
		}

		/* show window */
		hpsjam_client->show();

		return (app.exec());
	} else {
		QCoreApplication app(argc, argv);

		hpsjam_server_peers = new class hpsjam_server_peer [hpsjam_num_server_peers];

		/* set a valid UDP buffer size */
		hpsjam_udp_buffer_size = 2000 * HPSJAM_SEQ_MAX * hpsjam_num_server_peers;

		/* create sockets, if any */
		hpsjam_socket_init(ipv4_port, ipv6_port, cliport);

		/* create timer, if any */
		hpsjam_timer_init();

		/* prevent system from sleeping this program */
#if defined(Q_OS_MACX)
		HpsJamBeginActivity();
#endif
		return (app.exec());
	}
}

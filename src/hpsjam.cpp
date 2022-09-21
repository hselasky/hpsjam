/*-
 * Copyright (c) 2020-2021 Hans Petter Selasky. All rights reserved.
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
#include "httpd.h"

#include "../mac/activity.h"

#include <QApplication>
#include <QCoreApplication>
#include <QMessageBox>
#include <QThread>

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <err.h>

#ifdef __FreeBSD__
#include <sys/filio.h>
#include <sys/rtprio.h>
#endif

unsigned hpsjam_num_server_peers;
unsigned hpsjam_udp_buffer_size;
unsigned hpsjam_num_cpu = 1;
uint64_t hpsjam_server_passwd;
uint64_t hpsjam_mixer_passwd;
bool hpsjam_mute_peer_audio;
class hpsjam_server_peer *hpsjam_server_peers;
class hpsjam_client_peer *hpsjam_client_peer;
class HpsJamClient *hpsjam_client;
struct hpsjam_socket_address hpsjam_v4[HPSJAM_PORTS_MAX];
struct hpsjam_socket_address hpsjam_v6[HPSJAM_PORTS_MAX];
struct hpsjam_socket_address hpsjam_cli;
const char *hpsjam_welcome_message_file;
int hpsjam_profile_index;
bool hpsjam_no_multi_port;

static const struct option hpsjam_opts[] = {
	{ "NSDocumentRevisionsDebugMode", required_argument, NULL, ' ' },
	{ "profile", required_argument, NULL, 'f' },
	{ "port", required_argument, NULL, 'p' },
	{ "no-multi-port", no_argument, NULL, 'm' },
	{ "cli-port", required_argument, NULL, 'q' },
	{ "help", no_argument, NULL, 'h' },
	{ "ncpu", required_argument, NULL, 'j' },
	{ "platform", required_argument, NULL, ' ' },
	{ "mute-peer-audio", no_argument, NULL, 'g' },
#ifdef HAVE_HTTPD
	{ "httpd", required_argument, NULL, 't' },
	{ "httpd-conns", required_argument, NULL, 'T' },
#endif
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
#if defined(HAVE_MAC_AUDIO) || defined(HAVE_ASIO_AUDIO)
	{ "audio-input-device", required_argument, NULL, 'I'},
	{ "audio-output-device", required_argument, NULL, 'O'},
	{ "audio-input-left", required_argument, NULL, 'l'},
	{ "audio-output-left", required_argument, NULL, 'L'},
	{ "audio-input-right", required_argument, NULL, 'r'},
	{ "audio-output-right", required_argument, NULL, 'R'},
	{ "audio-buffer-samples", required_argument, NULL, 'b'},
	{ "midi-port-name", required_argument, NULL, 'n'},
#endif
#ifdef HAVE_JACK_AUDIO
	{ "jacknoconnect", no_argument, NULL, 'J' },
	{ "jackname", required_argument, NULL, 'n' },
#endif
	{ "audio-input-jitter", required_argument, NULL, 'v'},
	{ "audio-output-jitter", required_argument, NULL, 'V'},
#ifdef __FreeBSD__
	{ "rtprio", required_argument, NULL, 'x' },
#endif
	{ NULL, 0, NULL, 0 }
};

static void
usage(void)
{
        fprintf(
#ifdef _WIN32
		stdout,
#else
		stderr,
#endif
		"HpsJam [--server --peers <1..256>] [--port " HPSJAM_DEFAULT_PORT_STR "] \\\n"
		"	[--no-multi-port] \\\n"
#ifndef _WIN32
		"	[--daemon] \\\n"
#endif
		"	[--help] \\\n"
		"	[--profile <positive value, Default is 0>] \\\n"
		"	[--password <64_bit_hexadecimal_password>] \\\n"
#ifdef HAVE_JACK_AUDIO
		"	[--jacknoconnect] [--jackname <name>] \\\n"
#endif
		"	[--nickname <nickname>] \\\n"
		"	[--icon <0..%u>] \\\n"
		"	[--connect <servername:port>] \\\n"
#ifdef HAVE_HTTPD
		"	[--httpd <servername:port, Default port is 80>] \\\n"
		"	[--httpd-conns <max number of connections, Default is 1> \\\n"
#endif
		"	[--audio-uplink-format <0..%u>] \\\n"
		"	[--audio-downlink-format <0..%u>] \\\n"
		"	[--audio-input-jitter <0..%u milliseconds, Default is 8 ms>] \\\n"
		"	[--audio-output-jitter <0..%u milliseconds, Default is 8 ms>] \\\n"
#if defined(HAVE_MAC_AUDIO) || defined(HAVE_ASIO_AUDIO)
		"	[--audio-input-device <0,1,2,3 ... , Default is 0>] \\\n"
		"	[--audio-output-device <0,1,2,3 ... , Default is 0>] \\\n"
		"	[--audio-input-left <0,1,2,3 ... , Default is 0>] \\\n"
		"	[--audio-output-left <0,1,2,3 ... , Default is 0>] \\\n"
		"	[--audio-input-right <0,1,2,3 ... , Default is 1>] \\\n"
		"	[--audio-output-right <0,1,2,3 ... , Default is 1>] \\\n"
		"	[--audio-buffer-samples <Default is 96>] \\\n"
		"	[--midi-port-name <name>, Default is hpsjam] \\\n"
#endif
		"	[--mixer-password <64_bit_hexadecimal_password>] \\\n"
		"	[--ncpu <1,2,3, ... %d, Default is 1>] \\\n"
		"	[--platform offscreen] \\\n"
		"	[--mute-peer-audio] \\\n"
		"	[--welcome-msg-file <filename> \\\n"
#ifdef __FreeBSD__
		"	[--rtprio <priority>] \\\n"
#endif
		"	[--cli-port <portnumber>]\n",
		HPSJAM_NUM_ICONS - 1,
		HPSJAM_AUDIO_FORMAT_MAX - 1,
		HPSJAM_AUDIO_FORMAT_MAX - 1,
		HPSJAM_MAX_SAMPLES / HPSJAM_DEF_SAMPLES / 2,
		HPSJAM_MAX_SAMPLES / HPSJAM_DEF_SAMPLES / 2,
		HPSJAM_CPU_MAX);
        exit(1);
}

Q_DECL_EXPORT int
main(int argc, char **argv)
{
	static const char hpsjam_short_opts[] = {
	    "M:q:p:sP:hBJ:n:K:w:mN:gi:j:c:U:D:I:O:l:L:r:R:t:T:v:V:b:x:"
	};
	int c;
	int port = HPSJAM_DEFAULT_PORT;
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
#if defined(HAVE_MAC_AUDIO) || defined(HAVE_ASIO_AUDIO)
	int input_device = -1;
	int output_device = -1;
	int input_left = -1;
	int output_left = -1;
	int input_right = -1;
	int output_right = -1;
	int buffer_samples = -1;
#endif
	int input_jitter = -1;
	int output_jitter = -1;

	while ((c = getopt_long_only(argc, argv, hpsjam_short_opts, hpsjam_opts, NULL)) != -1) {
		switch (c) {
		case 'f':
			hpsjam_profile_index = atoi(optarg);
			if (hpsjam_profile_index < 0)
				usage();
			break;
#ifdef HAVE_HTTPD
		case 't': {
			char *ptr;

			http_host = optarg;
			ptr = strchr(optarg, ':');
			if (ptr == 0) {
				http_port = "80";
			} else {
				*ptr++ = 0;
				http_port = ptr;
			}
			if (http_nstate == 0)
				http_nstate = 1;
			break;
		}
		case 'T':
			http_nstate = atoi(optarg);
			if (http_nstate == 0 || http_host == 0 || http_port == 0)
				usage();
			break;
#endif
		case 'w':
			hpsjam_welcome_message_file = optarg;
			break;
		case 's':
			if (hpsjam_num_server_peers == 0)
				hpsjam_num_server_peers = 1;
			break;
		case 'p':
			port = atoi(optarg);
			if (port <= 0 || port >= 65536)
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
#if defined(HAVE_MAC_AUDIO) || defined(HAVE_ASIO_AUDIO)
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
		case 'b':
			buffer_samples = atoi(optarg);
			if (buffer_samples < 1 ||
			    buffer_samples > HPSJAM_MAX_BUFFER_SAMPLES)
				usage();
			break;
#endif
#ifndef _WIN32
		case 'B':
			do_fork = 1;
			break;
#endif
		case 'J':
			jackconnect = false;
			break;
		case 'j':
			hpsjam_num_cpu = atoi(optarg);
			if (hpsjam_num_cpu == 0)
				usage();
			else if (hpsjam_num_cpu > HPSJAM_CPU_MAX)
				usage();
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
		case 'g':
			hpsjam_mute_peer_audio = true;
			break;
		case 'v':
			input_jitter = atoi(optarg);
			if (input_jitter < 0)
				usage();
			break;
		case 'V':
			output_jitter = atoi(optarg);
			if (output_jitter < 0)
				usage();
			break;
		case 'm':
			hpsjam_no_multi_port = true;
			break;
		case ' ':
			/* ignore */
			break;
#ifdef __FreeBSD__
		case 'x': {
			struct rtprio rtp;
			memset(&rtp, 0, sizeof(rtp));
			rtp.type = RTP_PRIO_REALTIME;
			rtp.prio = atoi(optarg);
			if (rtprio(RTP_SET, getpid(), &rtp) != 0)
				printf("Cannot set realtime priority\n");
			break;
		}
#endif
		default:
			usage();
			break;
		}
	}

#ifndef _WIN32
	if (do_fork && daemon(0, 0) != 0)
		errx(1, "Cannot daemonize");
#endif

#ifdef HAVE_HTTPD
	/* start httpd server, if any */
	hpsjam_httpd_start();
#endif

	qRegisterMetaType<uint8_t>("uint8_t");

	if (hpsjam_num_server_peers == 0) {
		QApplication app(argc, argv);

		/* set consistent double click interval */
		app.setDoubleClickInterval(250);

#if defined(HAVE_MAC_AUDIO) || defined(HAVE_ASIO_AUDIO) || defined(HAVE_JACK_AUDIO)
		hpsjam_sound_rescan();
#endif
		hpsjam_default_midi = new hpsjam_midi_buffer[1];
		hpsjam_client_peer = new class hpsjam_client_peer;
		hpsjam_client = new HpsJamClient();

		/* load current settings, if any */
		hpsjam_client->loadSettings();

		if (input_jitter > -1)
			hpsjam_client->w_config->audio_dev.s_jitter_input.setValue(input_jitter);
		if (output_jitter > -1)
			hpsjam_client->w_config->audio_dev.s_jitter_output.setValue(output_jitter);

#if defined(HAVE_MAC_AUDIO) || defined(HAVE_ASIO_AUDIO)
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
		if (buffer_samples < 0)
			buffer_samples = hpsjam_client->buffer_samples;
#endif

#ifdef HAVE_JACK_AUDIO
		if (hpsjam_sound_init(jackname, jackconnect)) {
			QMessageBox::information(hpsjam_client, QObject::tr("NO AUDIO"),
				QObject::tr("Cannot connect to JACK server or \n"
					    "sample rate is different from %1Hz or \n"
					    "latency is too high").arg(HPSJAM_SAMPLE_RATE));
		}

		hpsjam_client->w_config->audio_dev.refreshStatus();
#endif

#ifdef HAVE_MAC_AUDIO
		/* setup MIDI first */
		hpsjam_midi_init(jackname);

		if (input_device > -1 || output_device > -1) {
			if (input_device > -1 && hpsjam_client->w_config->audio_dev.handle_set_input_device(input_device) < 0) {
				QMessageBox::information(hpsjam_client, QObject::tr("NO AUDIO"),
				    QObject::tr("Cannot find the specified audio input device"));
			}
			if (output_device > -1 && hpsjam_client->w_config->audio_dev.handle_set_output_device(output_device) < 0) {
				QMessageBox::information(hpsjam_client, QObject::tr("NO AUDIO"),
				    QObject::tr("Cannot find the specified audio output device"));
			}
		} else {
			if (hpsjam_sound_init(0, 0)) {
				QMessageBox::information(hpsjam_client, QObject::tr("NO AUDIO"),
				    QObject::tr("Cannot connect to audio subsystem.\n"
						"Check that you have an audio device connected and\n"
						"that the sample rate is set to %1Hz.").arg(HPSJAM_SAMPLE_RATE));
			}
		}

		if (buffer_samples > 0)
			hpsjam_client->w_config->audio_dev.handle_toggle_buffer_samples(buffer_samples);
		if (output_left > -1)
			hpsjam_client->w_config->audio_dev.handle_set_output_left(output_left + 1);
		if (output_right > -1)
			hpsjam_client->w_config->audio_dev.handle_set_output_right(output_right + 1);
		if (input_left > -1)
			hpsjam_client->w_config->audio_dev.handle_set_input_left(input_left + 1);
		if (input_right > -1)
			hpsjam_client->w_config->audio_dev.handle_set_input_right(input_right + 1);

		hpsjam_client->w_config->audio_dev.refreshStatus();
#endif

#ifdef HAVE_ASIO_AUDIO
		if (input_device > -1) {
			if (hpsjam_client->w_config->audio_dev.handle_set_input_device(input_device) < 0) {
				QMessageBox::information(hpsjam_client, QObject::tr("NO AUDIO"),
				    QObject::tr("Cannot find the specified audio device"));
			}
		} else {
			if (hpsjam_sound_init(0, 0)) {
				QMessageBox::information(hpsjam_client, QObject::tr("NO AUDIO"),
				    QObject::tr("Cannot connect to ASIO subsystem or \n"
						"sample rate is different from %1Hz.").arg(HPSJAM_SAMPLE_RATE));
			}
		}

		if (buffer_samples > 0)
			hpsjam_client->w_config->audio_dev.handle_toggle_buffer_samples(buffer_samples);
		if (output_left > -1)
			hpsjam_client->w_config->audio_dev.handle_set_output_left(output_left + 1);
		if (output_right > -1)
			hpsjam_client->w_config->audio_dev.handle_set_output_right(output_right + 1);
		if (input_left > -1)
			hpsjam_client->w_config->audio_dev.handle_set_input_left(input_left + 1);
		if (input_right > -1)
			hpsjam_client->w_config->audio_dev.handle_set_input_right(input_right + 1);

		hpsjam_client->w_config->audio_dev.refreshStatus();
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

		hpsjam_client->w_connect->buttons.l_multi_port.setCurrentIndex(
			hpsjam_no_multi_port ? 1 : 0);

		/* set a valid UDP buffer size */
		hpsjam_udp_buffer_size = 2000 * HPSJAM_PORTS_MAX;

		/* create sockets, if any */
		hpsjam_socket_init(port, cliport);

		/* create timer, if any */
		hpsjam_timer_init();

		if (connect_to != 0) {
			/* wait a bit for sockets and timer to be created */
			usleep(250000);
			hpsjam_client->w_connect->handle_connect();
		}

		/* show window */
		hpsjam_client->show();

		/* run graphics at lower priority */
		QThread::currentThread()->setPriority(QThread::LowPriority);

		return (app.exec());
	} else {
		QCoreApplication app(argc, argv);

		hpsjam_default_midi = new hpsjam_midi_buffer[hpsjam_num_cpu];
		hpsjam_server_peers = new class hpsjam_server_peer [hpsjam_num_server_peers];

		for (unsigned x = 0; x != hpsjam_num_server_peers; x++) {
			if (output_jitter > -1) {
				hpsjam_server_peers[x].out_buffer[0].setWaterTarget(output_jitter);
				hpsjam_server_peers[x].out_buffer[1].setWaterTarget(output_jitter);
			} else {
				hpsjam_server_peers[x].out_buffer[0].setWaterTarget(8);
				hpsjam_server_peers[x].out_buffer[1].setWaterTarget(8);
			}

			if (input_jitter > -1) {
				hpsjam_server_peers[x].in_audio[0].setWaterTarget(input_jitter);
				hpsjam_server_peers[x].in_audio[1].setWaterTarget(input_jitter);
			} else {
				hpsjam_server_peers[x].in_audio[0].setWaterTarget(8);
				hpsjam_server_peers[x].in_audio[1].setWaterTarget(8);
			}
		}

		/* set a valid UDP buffer size */
		hpsjam_udp_buffer_size = 2000 * HPSJAM_PORTS_MAX;

		/* create sockets, if any */
		hpsjam_socket_init(port, cliport);

		/* create timer, if any */
		hpsjam_timer_init();

		/* prevent system from sleeping this program */
#if defined(Q_OS_MACX)
		HpsJamBeginActivity();
#endif
		return (app.exec());
	}
}

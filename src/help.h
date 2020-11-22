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

#ifndef _HPSJAM_HELP_H_
#define	_HPSJAM_HELP_H_

#include <QObject>
#include <QPlainTextEdit>

class HpsJamHelp : public QPlainTextEdit {
public:
	HpsJamHelp() {
	  setReadOnly(true);
	  setPlainText(tr(
        "### SHORTCUTS / HOTKEYS for hpsjam ###"
	"\n"
	"\n"
	"### SELECT GLOBAL TABS ###\n"
        "    CONNECT: ALT+E\n"
        "    MIXER: ALT+M\n"
        "    LYRICS: ALT+L\n"
        "    CHAT: ALT+A\n"
        "    CONFIG:  ALT+F\n"
        "    STATS: ALT+S\n"
        "\n"
        "## INSIDE CONNECT TAB ##\n"
        "    Connect: ALT+C\n"
        "    Disconnect: ALT+D\n"
        "    Refresh: ALT+R\n"
        "\n"
        "## INSIDE MIXER TAB ##\n"
        "    # Local Mix\n"
        "        Pan to left: L\n"
        "        Pan to right: R\n"
        "        Mute myself: M\n"
        "        Negate or invert audio: I\n"
        "        Select balance between local and remote audio: 1 to 9\n"
        "        - Only hear remote audio: 1\n"
        "        - Hear both remote and local audio: 4\n"
        "        - Only hear local microphoneaudio: 9\n"
        "        Show EQ DELAY popup: E\n"
        "            # INSIDE EQ DELAY POPUP\n"
        "                Show default ISO template filter: ALT+U\n"
        "                Show lowpass filter: ALT+L\n"
        "                Show bandpass filter: ALT+B\n"
        "                Show long delay filter: ALT+Y\n"
        "                Show disabled filter: ALT+D\n"
        "                Close this window: ALT+D or Escape\n"
        "                Apply currently selected filter: ALT+A\n"
        "    # Server Mix\n"
        "        Mute myself: P\n"
        "\n"
        "# INSIDE CONFIG TAB #\n"
        "    Disable all audio: 0\n"
        "    8-bit mono: 1\n"
        "    8-bit stereo: 2\n"
        "    16-bit mono: 3\n"
        "    16-bit stereo: 4\n"
        "    24-bit mono: 5\n"
        "    24-bit stereo: 6\n"
        "    32-bit mono: 7\n"
        "    32-bit stereo: 8\n"
	"    Toggle audio input device: I\n"
	"    Toggle audio output device: O\n"));
    };
};

#endif		/* _HPSJAM_HELP_H_ */

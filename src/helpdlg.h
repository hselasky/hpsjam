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

#ifndef _HPSJAM_HELP_H_
#define	_HPSJAM_HELP_H_

#include <QObject>
#include <QPlainTextEdit>

class HpsJamHelp : public QPlainTextEdit {
public:
	HpsJamHelp() {
	  setReadOnly(true);
	  setPlainText(tr(
	"### SHORTCUTS / HOTKEYS for hpsjam ###\n"
	"\n"
	"On the Mac the ALT key equals the option key.\n"
	"\n"
	"### SELECT GLOBAL TABS ###\n"
        "    SERVER: ALT+E\n"
        "    MIXER: ALT+M\n"
        "    LYRICS: ALT+L\n"
        "    CHAT: ALT+A\n"
        "    CONFIG:  ALT+F\n"
        "    STATS: ALT+S\n"
        "\n"
        "## INSIDE SERVER TAB ##\n"
        "    Connect: ALT+O\n"
        "    Disconnect: ALT+D\n"
        "    Reset server list: ALT+R\n"
        "\n"
	"## INSIDE CHAT TAB ##\n"
	"    Send a line of lyrics: ALT+Y\n"
	"    Clear chat window: ALT+R\n"
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
        "    # Status characters used after Mix<number>\n"
        "        I: Local audio mix is inverted else normal\n"
        "        M: Local audio mix is muted else unmuted\n"
        "        S: This mix is selected for solo else normal\n"
        "        E: Master audio equalizer is active else disabled\n"
        "        L: Master audio is panned to left else in center\n"
        "        R: Master audio is panned to right else in center\n"
        "        G: Master audio is gained else full gain\n"
        "        +1 to +15: Positive local gain, else no gain\n"
        "        -1 to -15: Negative local gain, else no gain\n"
        "    # Status characters used after Balance\n"
        "        I: Local audio is inverted else normal\n"
        "        M: Your audio is muted else normal\n"
        "        E: Local audio equalizer is active else disabled\n"
        "        L: Local audio is panned to left else in center\n"
        "        R: Local audio is panned to right else in center\n"
        "        G: Local audio is gained else full gain\n"
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

	setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    };
};

#endif		/* _HPSJAM_HELP_H_ */

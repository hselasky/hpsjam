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
	    "List of key shortcuts for window selection:\n"
	    "ALT+E: Select connect window\n"
	    "ALT+M: Select mixer window\n"
	    "ALT+L: Select lyrics window\n"
	    "ALT+A: Select chat window\n"
	    "ALT+F: Select config window\n"
	    "ALT+S: Select statistics window\n"
	    "ALT+H: Select help window\n"
	    "\n"
	    "List of key shortcuts for mixer window:\n"
	    "L: Pan local fader to left\n"
	    "R: Pan local fader  to right\n"
	    "M: Mute all locally transmitted audio\n"
	    "I: Negate or invert locally transmitted audio\n"
	    "E: Show equalizer dialog\n"
	    "P: Mute myself from received remote audio\n"
	    "1 to 9: Select balance between local and remote audio\n"
	    "1: Only hear remote audio\n"
	    "4: Hear both remote and local audio\n"
	    "9: Only hear local microphone audio\n"
	    "\n"
	    "List of key shortcuts for equalizer popup window:\n"
	    "ALT+U: Show default ISO template filter\n"
	    "ALT+L: Show lowpass filter\n"
	    "ALT+B: Show bandpass filter\n"
	    "ALT+Y: Show long delay filter\n"
	    "ALT+D: Show disabled filter\n"
	    "ALT+C or Escape: Close the window\n"
	    "ALT+A: Apply currently selected filter\n"
	    "\n"
	    "List of key shortcuts for connect window:\n"
	    "ALT+C: Connect to selected server\n"
	    "ALT+D: Disconnect from server\n"
	    "ALT+R: Refresh server list\n"
	    "\n"
	    "List of key shortcuts for config window:\n"
	    "0: Disable all audio\n"
	    "1: 8-bit mono\n"
	    "2: 8-bit stereo\n"
	    "3: 16-bit mono\n"
	    "4: 16-bit stereo\n"
	    "5: 24-bit mono\n"
	    "6: 24-bit stereo\n"
	    "7: 32-bit mono\n"
	    "8: 32-bit stereo\n"));
	};
};

#endif		/* _HPSJAM_HELP_H_ */

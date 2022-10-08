<IMG SRC="https://raw.githubusercontent.com/hselasky/hpsjam/main/HpsJam.svg"></IMG> 
# HPS JAM
An online audio collaboration tool for low latency audio with lyrics and chat support.

# News
Some non-backwards compatible network protocol changes have been
introduced as of v1.1.0 to significantly reduce audio jitter.
Please update both client and server software at the same time!

# Features
<ul>
  <li>uncompressed audio transmission in 1 channel 8-bit up to 2 channels 32-bit. This results in crystal clear high-end audio over the internet!</li>
  <li>additional protection against jitter by redundancy in packet transmission</li>
  <li>local audio effects:
    <ul>
      <li>highpass</li>
      <li>lowpass</li>
      <li>bandpass</li>
      <li>delay - With this feature you can hear yourself with an "average" delay that fits the real one from the server. This provides a clean local signal without jitter, while maintaining the delay and thus being "in sync" with fellow musicians when playing/jamming. Usage: click "MIXER" then "EQ DELAY" then "Long Delay" and then "Apply". This will apply the calculated average. You may want to click the "Long Delay" button several times before hitting apply, so that you can choose the appropiate delay. Credits go to <A HREF="https://github.com/dingodoppelt">Nils Brederlow</A> for suggesting it.</li>
    </ul>
  </li>
  <li>automatic audio compression when signal overflows.</li>
  <li>built in HTTP server allows for streaming the uncompressed audio
  in 32-bit stereo WAV-file format to disk or other programs. Supported for
  both client and server.</li>
  <li>server and mixer access can be password protected.</li>
  <li>low latency MIDI event routing</li>
</ul>

## How to build client and server under FreeBSD
<ul>
  <li>qmake PREFIX=/usr/local</li>
  <li>make all</li>
  <li>make install</li>
</ul>

## How to build server under Ubuntu Linux
<ul>
  <li>sudo apt-get install git build-essential qt5-qmake qtbase5-dev qtbase5-dev-tools libqt5svg5-dev libqt5webenginewidgets5 libqt5webchannel5-dev qtwebengine5-dev libjack-dev jackd</li>
  <li>qmake PREFIX=/usr WITHOUT_AUDIO=YES QMAKE_CFLAGS_ISYSTEM="-I"</li>
  <li>make all</li>
  <li>make install</li>
</ul>

NOTE 1) By giving qmake the "WITHOUT_AUDIO=YES" flag you can skip the jack dependency for the server side.
NOTE 2) By giving qmake the "QMAKE_CFLAGS_ISYSTEM=-I" flag you can fix the following compile error "fatal error: stdlib.h: No such file or directory"

## Dependencies
<ul>
<li><A HREF="https://jackaudio.org">jackd libjack-dev</A></li>
<li><A HREF="http://www.asio4all.org">ASIO</A></li>
<li>build-essential</li>
<li>qt5-qmake</li>
<li>qtbase5-dev</li>
<li>qtbase5-dev-tools</li>
<li>libqt5svg5-dev</li>
<li>libqt5webenginewidgets5</li>
<li>libqt5webchannel5-dev</li>
<li>qtwebengine5-dev</li>
</ul>

## Example how to start the client
<pre>
HpsJam &
</pre>

## Example how to start the server in foreground mode, to see errors
<pre>
HpsJam --server --port 22124 --peers 16
</pre>

## Example how to start the server
<pre>
HpsJam --server --port 22124 --peers 16 --daemon
</pre>

## Example of an Ubuntu service file
<pre>
[Unit]
Description=HpsJam headless server
After=network.target
StartLimitIntervalSec=0

[Service]
Type=simple
User=jamulus # or hpsjam if that exists
Group=nogroup
NoNewPrivileges=true
ProtectSystem=true
ProtectHome=true
Nice=-20
IOSchedulingClass=realtime
IOSchedulingPriority=0

#### Change this parameters to your liking
ExecStart=/bin/sh -c 'exec /usr/bin/HpsJam --server \
--port 22126 \
--peers 16'
# [--welcome-msg-file /yourPath/yourFile
#	[--password <64_bit_hexadecimal_password>] \
#	[--mixer-password <64_bit_hexadecimal_password>] \
#	[--ncpu <1,2,3, ... 64, Default is 1>] \
#	[--httpd <servername:port, Default is [--httpd 127.0.0.1:80>] \
#	[--httpd-conns <max number of connections, Default is 1> \
#	[--cli-port <portnumber>]

Restart=on-failure
RestartSec=30
StandardOutput=journal
StandardError=inherit
SyslogIdentifier=jamulus

[Install]
WantedBy=multi-user.target
#
</pre>

## Example how to use ffmpeg to stream from HpsJam to icecast
<pre>
HpsJam --server --port 22124 --peers 16 --httpd 127.0.0.1:8080 --daemon

ffmpeg -f s32le -ac 2 -ar 48000 -i http://127.0.0.1:8080/stream.wav \
       -acodec libmp3lame -ab 128k -ac 2 -content_type audio/mpeg \
       -f mp3 icecast://source:yourpassword@127.0.0.1:8000/stream
</pre>

## How to get help about the commandline parameters
<pre>
HpsJam -h
</pre>

## Privacy policy

HPS JAM does not collect any information from its users.

## Supported platforms
<ul>
  <li>FreeBSD <A HREF="https://www.freshports.org/audio/hpsjam">hpsjam port</A></li>
  <li>Linux</li>
  <li>iOS (See App Store)</li>
  <li>MacOSX <A HREF="http://www.selasky.org/downloads/HpsJam.dmg">Binary build here</A> (Use Safari to download)</li>
  <li>Windows (64-bit) <A HREF="http://www.selasky.org/downloads/hpsjam-binary-win64.zip">Binary build here</A></li>
  <li>Raspberry-Pi <A HREF="https://github.com/kdoren/jambox-pi-gen">JamBox</A></li>
</ul>

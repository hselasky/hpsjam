<IMG SRC="https://raw.githubusercontent.com/hselasky/hpsjam/main/HpsJam.svg"></IMG> 
# HPS JAM
An online audio collaboration tool for low latency audio with lyrics and chat support.

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

## How to build
<ul>
  <li>qmake PREFIX=/usr # Linux</li> Also see more detailed instruction below.
  <li>qmake PREFIX=/usr/local # FreeBSD</li>
  <li>make all</li>
  <li>make install</li>
</ul>

## Dependencies
<ul>
  <li> QT core</li>
  <li> QT GUI</li>
  <li> QT widgets</li>
  <li> QT SVG renderer</li>
  <li> <A HREF="http://www.fftw.org">FFTW3</A> </li>
  <li> <A HREF="https://jackaudio.org">JACK</A> </li>
  <li> <A HREF="http://www.asio4all.org">ASIO</A> </li>
</ul>

## SERVER COMPILE AND INSTALL ON UBUNTU LINUX

In the hope that my experiences with the compilation issues might help others down the line, here comes my (probably very imperfect How2)

<ul>

----
> IMPORTANT PREVIOUS NOTES:
1) By giving qmake this flag: qmake WITHOUT_AUDIO=YES
You can skip the jack dependency for the server side.

2) By giving qmake this flag: 'QMAKE_CFLAGS_ISYSTEM=-I'
You can avoid the error "/usr/include/c++/9/cstdlib:75:15: fatal error: stdlib.h: No such file or directory
75 | #include_next <stdlib.h>"
----

> STEPS TO COMPILE AND INSTALL

sudo apt-get install git build-essential qt5-qmake qtbase5-dev qtbase5-dev-tools libqt5svg5-dev libqt5webenginewidgets5 libqt5webchannel5-dev qtwebengine5-dev libfftw3-dev libjack-dev jackd

cd /home/YOUR_USERNAME
clone https://github.com/hselasky/hpsjam
cd hpsjam
qmake 'QMAKE_CFLAGS_ISYSTEM=-I' 'WITHOUT_AUDIO=YES' PREFIX=/usr
(wait to compile. This can take some time. Ignore any warnings)
make all
sudo make install

> NOW TO START THE SERVER:

1) First start it without daemon, to be able to see any messages
HpsJam --server --port 22124 --peers 16 --httpd 127.0.0.1:8080

If you happen to see this error message: "HpsJam: Cannot bind to IP port: Address already in use", try to change the port e.g. "--port 22125"

2) If all good, start it daemonized
HpsJam --server --port 22125 --peers 16 --httpd 127.0.0.1:8080 --daemon

3) For details on server parameters, issue "HpsJam --help"

-------------------------------------------------------
-------------------------------------------------------

CLIENT COMPILE AND INSTALL ON UBUNTU LINUX

sudo apt install git build-essential qt5-qmake qtbase5-dev qtbase5-dev-tools libqt5svg5-dev libqt5webenginewidgets5 libqt5webchannel5-dev qtwebengine5-dev libfftw3-dev libjack-dev jackd


clone https://github.com/hselasky/hpsjam
cd hpsjam
qmake 'QMAKE_CFLAGS_ISYSTEM=-I' PREFIX=/usr
(wait to compile. This can take some time. Ignore any warnings)
make all
sudo make install

</ul>


## Example how to start the client
<pre>
HpsJam
</pre>

## Example how to start the server
<pre>
HpsJam --server --port 22124 --peers 16 --daemon
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
  <li>MacOSX <A HREF="http://home.selasky.org/privat/HpsJam.dmg">Binary build here</A> (Use Safari to download)</li>
  <li>Windows (64-bit) <A HREF="http://home.selasky.org/privat/hpsjam-binary-win64.zip">Binary build here</A></li>
  <li>Raspberry-Pi <A HREF="https://github.com/kdoren/jambox-pi-gen">JamBox</A></li>
</ul>


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
</ul>

## How to build
<ul>
  <li>qmake PREFIX=/usr # Linux</li>
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
  <li> FFTW3 </li>
  <li> JACK </li>
</ul>

<b>NOTE:</b> for those on Linux that have Jamulus already installed, you need to install additionally:
<ul>
  <li>libfftw3-dev</li>
  <li>libqt5svg5-dev</li>
</ul>
This command should do the trick:
<pre>
sudo apt-get install libfftw3-dev libqt5svg5-dev
</pre>

## Example how to start the client
<pre>
HpsJam
</pre>

## Example how to start the server
<pre>
HpsJam --server --port 22124 --peers 16 --daemon
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
  <li>MacOSX <A HREF="http://home.selasky.org/privat/HpsJam.dmg">Binary build here</A></li>
</ul>

<IMG SRC="https://raw.githubusercontent.com/hselasky/hpsjam/main/HpsJam.svg"></IMG> 
# HPS JAM
An online audio collaboration tool for low latency audio with lyrics and chat support.

# Features
- uncompressed audio transmission in 1CH@8bit up to 2CH@32bit. This results in crystal clear high-end audio over the internet!!!
- additional protection against jitter by redundancy in packet transmission
- local audio effects:
-- highpass
-- lowpass
-- bandpass
-- delay (with this feature you can hear yourself with an "average" delay that fits the real one from the server. This provides a clean local signal without jitter, while maintaining the delay and thus being "in sync" with fellow musicians when playing/jamming). Usage: click MIXER -> EQ DELAY -> "Long Delay" and then "apply". This will apply the calculated average. You may want to click the "Long Delay" button several times before hitting apply, so that you can chosse the appropiate delay.


## How to build
<ul>
  <li>qmake</li>
  <li>make</li>
</ul>

## Dependencies
<ul>
  <li> QT core</li>
  <li> QT GUI</li>
  <li> QT widgets</li>
  <li> QT SVG renderer</li>
  <li>FFTW3</li>
</ul>

Note: for those on Linux that have Jamulus already installed, you need to install additionally: libfftw3-dev and libqt5svg5-dev. This command would do the trick:
sudo apt-get install libfftw3-dev libqt5svg5-dev

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

## Supported platforms
<ul>
  <li>FreeBSD</li>
  <li>Linux</li>
  <li>MacOSX <A HREF="http://home.selasky.org/privat/HpsJam.dmg">Binary build here</A></li>
</ul>

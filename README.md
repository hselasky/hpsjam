<IMG SRC="https://raw.githubusercontent.com/hselasky/hpsjam/main/HpsJam.svg"></IMG> 
# HPS JAM
An online audio collaboration tool for low latency audio with lyrics and chat support.

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

## Example how to start client
<pre>
HpsJam
</pre>

## Example how to start server
<pre>
HpsJam --server --port 22124 --peers 16 --daemon
</pre>

## How to get more help
<pre>
HpsJam -h
</pre>

## Supported platforms
<ul>
  <li>FreeBSD</li>
  <li>Linux</li>
  <li>MacOSX <A HREF="http://home.selasky.org/privat/HpsJam.dmg">Binary build here</A></li>
</ul>

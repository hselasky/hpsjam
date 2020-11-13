#
# QMAKE project file for HPSJAM
#
TEMPLATE	= app
CONFIG		+= qt release
QT		+= core gui svg widgets

HEADERS		+= src/audiobuffer.h
HEADERS		+= src/chatdlg.h
HEADERS		+= src/clientdlg.h
HEADERS		+= src/compressor.h
HEADERS		+= src/configdlg.h
HEADERS		+= src/connectdlg.h
HEADERS		+= src/eqdlg.h
HEADERS		+= src/equalizer.h
HEADERS		+= src/hpsjam.h
HEADERS		+= src/jitter.h
HEADERS		+= src/lyricsdlg.h
HEADERS		+= src/mixerdlg.h
HEADERS		+= src/multiply.h
HEADERS		+= src/peer.h
HEADERS		+= src/protocol.h
HEADERS		+= src/socket.h
HEADERS		+= src/statsdlg.h
HEADERS		+= src/timer.h
HEADERS		+= src/volumedlg.h

SOURCES		+= src/audiobuffer.cpp
SOURCES		+= src/chatdlg.cpp
SOURCES		+= src/clientdlg.cpp
SOURCES		+= src/compressor.cpp
SOURCES		+= src/configdlg.cpp
SOURCES		+= src/connectdlg.cpp
SOURCES		+= src/eqdlg.cpp
SOURCES		+= src/equalizer.cpp
SOURCES		+= src/hpsjam.cpp
SOURCES		+= src/jitter.cpp
SOURCES		+= src/lyricsdlg.cpp
SOURCES		+= src/mixerdlg.cpp
SOURCES		+= src/multiply.cpp
SOURCES		+= src/peer.cpp
SOURCES		+= src/protocol.cpp
SOURCES		+= src/socket.cpp
SOURCES		+= src/statsdlg.cpp
SOURCES		+= src/timer.cpp
SOURCES		+= src/volumedlg.cpp

isEmpty(WITHOUT_AUDIO) {

# MacOS audio backend
macx {
SOURCES		+= mac/sound_mac.cpp
LIBS+=		-framework AudioUnit
LIBS+=		-framework CoreAudio
DEFINES		+= HAVE_MAC_AUDIO
}

# JACK audio backend
!macx {
SOURCES		+= linux/sound_jack.cpp
LIBS		+= -L$${PREFIX}/lib -ljack
DEFINES		+= HAVE_JACK_AUDIO
}

}

RESOURCES	+= HpsJam.qrc

TARGET		= HpsJam

macx {
INCLUDEPATH 	+= /opt/local/include
LIBS		+= /opt/local/lib/libfftw3.a
}

!macx {
INCLUDEPATH     += $${PREFIX}/include
LIBS		+= -L$${PREFIX}/lib -lfftw3
}

LIBS		+= -pthread

target.path	= $${PREFIX}/bin
INSTALLS	+= target

macx {
icons.path= $${DESTDIR}/Contents/Resources
icons.files= HpsJam.icns
QMAKE_BUNDLE_DATA+= icons
}

!macx:!android:!ios:!win32:unix {
icons.path	= $${PREFIX}/share/pixmaps
icons.files	= HpsJam.png
INSTALLS	+= icons

desktop.path	= $${PREFIX}/share/applications
desktop.files	= HpsJam.desktop
INSTALLS	+= desktop
}

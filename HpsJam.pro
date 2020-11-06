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
HEADERS		+= src/equalizer.h
HEADERS		+= src/hpsjam.h
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
SOURCES		+= src/equalizer.cpp
SOURCES		+= src/hpsjam.cpp
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
# JACK audio backend
SOURCES		+= linux/sound_jack.cpp
LIBS		+= -L${PREFIX}/lib -ljack
DEFINES		+= HAVE_JACK_AUDIO
}

RESOURCES	+= HpsJam.qrc

TARGET		= HpsJam

LIBS		+= -L${PREFIX}/lib -lfftw3 -pthread

target.path	= $${PREFIX}/bin
INSTALLS	+= target

!macx:!android:!ios:!win32:unix {
icons.path	= $${PREFIX}/share/pixmaps
icons.files	= HpsJam.png
INSTALLS	+= icons

desktop.path	= $${PREFIX}/share/applications
desktop.files	= HpsJam.desktop
INSTALLS	+= desktop
}

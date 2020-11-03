#
# QMAKE project file for HPSJAM
#
TEMPLATE	= app
CONFIG		+= qt release
QT		+= core gui svg widgets

HEADERS		+= src/chatdlg.h
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

SOURCES		+= src/chatdlg.cpp
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

RESOURCES	+= HpsJam.qrc

TARGET		= HpsJam

LIBS		+= -L${PREFIX}/lib -lfftw3

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

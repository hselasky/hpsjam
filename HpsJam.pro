#
# QMAKE project file for HPSJAM
#
TEMPLATE	= app
CONFIG		+= qt release
QT		+= core gui widgets

HEADERS		+= src/compressor.h
HEADERS		+= src/hpsjam.h
HEADERS		+= src/peer.h
HEADERS		+= src/protocol.h
HEADERS		+= src/socket.h

SOURCES		+= src/compressor.cpp
SOURCES		+= src/hpsjam.cpp
SOURCES		+= src/peer.cpp
SOURCES		+= src/protocol.cpp
SOURCES		+= src/socket.cpp

RESOURCES	+= HpsJam.qrc

TARGET		= HpsJam

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

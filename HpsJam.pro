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
HEADERS		+= src/help.h
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
SOURCES		+= src/help.cpp
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

macx {
HEADERS		+= mac/activity.h
SOURCES		+= mac/activity.mm
}

isEmpty(WITHOUT_AUDIO) {

# ASIO audio backend
win32 {
DEFINES         -= UNICODE
SOURCES         += \
        windows/sound_asio.cpp \
        windows/ASIOSDK2/common/asio.cpp \
        windows/ASIOSDK2/host/asiodrivers.cpp \
        windows/ASIOSDK2/host/pc/asiolist.cpp
INCLUDEPATH     += \
        windows/ASIOSDK2/common \
        windows/ASIOSDK2/host \
        windows/ASIOSDK2/host/pc
DEFINES         += HAVE_ASIO_AUDIO
}

# MacOS audio backend
macx {
SOURCES		+= mac/sound_mac.cpp
LIBS+=		-framework AudioUnit
LIBS+=		-framework CoreAudio
DEFINES		+= HAVE_MAC_AUDIO
}

# JACK audio backend
!macx:!win32 {
SOURCES		+= linux/sound_jack.cpp
LIBS		+= -L$${PREFIX}/lib -ljack
DEFINES		+= HAVE_JACK_AUDIO
}

}

RESOURCES	+= HpsJam.qrc

TARGET		= HpsJam

win32 {
LIBS            += \
        -lole32 \
        -luser32 \
        -ladvapi32 \
        -lwinmm \
        -lws2_32
QMAKE_CXXFLAGS	+= -include winsock2.h
QMAKE_CXXFLAGS	+= -include windows.h
QMAKE_CXXFLAGS	+= -include ws2ipdef.h
QMAKE_CXXFLAGS	+= -include ws2tcpip.h
QMAKE_CXXFLAGS	+= -include winsock.h
INCLUDEPATH	+= windows/include
LIBS		+= $${_PRO_FILE_PWD_}/windows/lib/libfftw3.a
RC_FILE		= windows/mainicon.rc
}

macx {
QMAKE_INFO_PLIST += HpsJamMacOSX.plist
INCLUDEPATH 	+= /opt/local/include
LIBS		+= /opt/local/lib/libfftw3.a
}

!macx:!win32 {
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

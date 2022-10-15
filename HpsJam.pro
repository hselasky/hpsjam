#
# QMAKE project file for HPSJAM
#
TEMPLATE	= app
!ios {
CONFIG		+= qt release
}
QT		+= core gui svg widgets

isEmpty(PREFIX) {
PREFIX		= /usr/local
}

HEADERS		+= src/audiobuffer.h
HEADERS		+= src/chatdlg.h
HEADERS		+= src/clientdlg.h
HEADERS		+= src/compressor.h
HEADERS		+= src/configdlg.h
HEADERS		+= src/connectdlg.h
HEADERS		+= src/eqdlg.h
HEADERS		+= src/equalizer.h
HEADERS		+= src/helpdlg.h
HEADERS		+= src/hpsjam.h
HEADERS		+= src/jitter.h
HEADERS		+= src/lyricsdlg.h
HEADERS		+= src/midibuffer.h
HEADERS		+= src/mixerdlg.h
HEADERS		+= src/multiply.h
HEADERS		+= src/peer.h
HEADERS		+= src/protocol.h
HEADERS		+= src/socket.h
HEADERS		+= src/recordingdlg.h
HEADERS		+= src/statsdlg.h
HEADERS		+= src/timer.h
HEADERS		+= src/texture.h

SOURCES		+= src/audiobuffer.cpp
SOURCES		+= src/chatdlg.cpp
SOURCES		+= src/clientdlg.cpp
SOURCES		+= src/compressor.cpp
SOURCES		+= src/configdlg.cpp
SOURCES		+= src/connectdlg.cpp
SOURCES		+= src/eqdlg.cpp
SOURCES		+= src/equalizer.cpp
SOURCES		+= src/helpdlg.cpp
SOURCES		+= src/hpsjam.cpp
SOURCES		+= src/jitter.cpp
SOURCES		+= src/lyricsdlg.cpp
SOURCES		+= src/midibuffer.cpp
SOURCES		+= src/mixerdlg.cpp
SOURCES		+= src/multiply.cpp
SOURCES		+= src/peer.cpp
SOURCES		+= src/protocol.cpp
SOURCES		+= src/recordingdlg.cpp
SOURCES		+= src/socket.cpp
SOURCES		+= src/statsdlg.cpp
SOURCES		+= src/timer.cpp
SOURCES		+= src/texture.cpp

SOURCES		+= kissfft/kiss_fft.c

macx {
HEADERS		+= mac/activity.h
SOURCES		+= mac/activity.mm
}

unix {
DEFINES         += HAVE_HTTPD
HEADERS         += src/httpd.h
SOURCES         += src/httpd.cpp
}

INCLUDEPATH	+= kissfft

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
LIBS+=		-framework CoreMIDI
DEFINES		+= HAVE_MAC_AUDIO
}

# IOS audio backend
ios {
SOURCES		+= ios/sound_ios.mm
SOURCES		+= ios/activity.mm
HEADERS		+= ios/activity.h
LIBS+=		-framework CoreAudio
LIBS+=		-framework AudioToolbox
LIBS+=		-framework AVFoundation
LIBS+=		-framework CoreMIDI
DEFINES		+= HAVE_IOS_AUDIO
Q_ENABLE_BITCODE.name = ENABLE_BITCODE
Q_ENABLE_BITCODE.value = NO
QMAKE_MAC_XCODE_SETTINGS += Q_ENABLE_BITCODE
}

# JACK audio backend
!macx:!win32:!ios {
SOURCES		+= linux/sound_jack.cpp
LIBS		+= -L$${PREFIX}/lib -ljack
DEFINES		+= HAVE_JACK_AUDIO
}

}

!isEmpty(WITHOUT_AUDIO) {
SOURCES         += src/sound_dummy.cpp
}

RESOURCES	+= HpsJam.qrc

TARGET		= HpsJam

win32 {
CONFIG		+= staticlib
LIBS		+= \
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
RC_FILE		= windows/mainicon.rc
}

macx {
QMAKE_INFO_PLIST += mac/HpsJam.plist
INCLUDEPATH	+= mac
}

ios {
INCLUDEPATH	+= ios/include
INCLUDEPATH	+= ios
}

!macx:!win32:!ios {
INCLUDEPATH     += $${PREFIX}/include
}

LIBS		+= -pthread

target.path	= $${PREFIX}/bin
INSTALLS	+= target

ios {
QMAKE_ASSET_CATALOGS += ios/Assets.xcassets
QMAKE_INFO_PLIST= ios/HpsJam.plist
QMAKE_APPLE_DEVICE_ARCHS= arm64
QMAKE_IOS_DEPLOYMENT_TARGET= 13.0
}

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

#!/bin/bash
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

sip-build \
	--confirm-license \
	--qmake /usr/lib/qt6/bin/qmake \
	--qmake-setting "QMAKE_CFLAGS_RELEASE = $CFLAGS" \
	--qmake-setting "QMAKE_CXXFLAGS_RELEASE = $CXXFLAGS" \
	--qmake-setting "QMAKE_LFLAGS_RELEASE = $LDFLAGS" \
	--qmake-setting "QMAKE_RPATHDIR =" \
	--api-dir /usr/share/qt6/qsci/api/python \
	--disable QtPdf \
	--disable QtPdfWidgets \
	--build-dir build \
	--no-make \
	--verbose
make -C build
make -C build -j1 INSTALL_ROOT="$PKG" install
site=$(find "$PKG"/usr/lib -maxdepth 2 -type d -name site-packages)
for m in QtCore QtGui QtWidgets QtDBus QtNetwork QtQml QtQuick QtSvg QtOpenGL \
	QtMultimedia QtTextToSpeech QtWebChannel QtWebSockets QtPositioning \
	QtSerialPort QtSensors QtQuick3D QtRemoteObjects QtDesigner QtHelp \
	QtStateMachine QtSpatialAudio QtBluetooth QtNfc; do
	test -n "$(find "$site/PyQt6" -maxdepth 1 -name "$m.*.so")"
done
test -n "$(find "$site/dbus/mainloop" -maxdepth 1 -name 'pyqt6.*.so')"

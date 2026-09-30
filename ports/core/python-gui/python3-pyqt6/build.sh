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

# sip-build configures one module per Qt library it finds with qmake, so the
# set built follows what is installed. The Qt libraries named in depends are
# built; QtPdf is disabled because it lives in the QtWebEngine source, whose
# binding is python3-pyqt6-webengine, and QtBluetooth and QtNfc because Qt
# Connectivity is not ported. Named rather than probed, a module cannot appear
# or vanish with build order.
#
# --api-dir writes the PyQt6.api file QScintilla's Python lexer completes from.
sip-build \
	--confirm-license \
	--qmake /usr/lib/qt6/bin/qmake \
	--qmake-setting "QMAKE_CFLAGS_RELEASE = $CFLAGS" \
	--qmake-setting "QMAKE_CXXFLAGS_RELEASE = $CXXFLAGS" \
	--qmake-setting "QMAKE_LFLAGS_RELEASE = $LDFLAGS" \
	--api-dir /usr/share/qt6/qsci/api/python \
	--disable QtPdf \
	--disable QtPdfWidgets \
	--disable QtBluetooth \
	--disable QtNfc \
	--build-dir build \
	--no-make \
	--verbose
make -C build

# The generated install rules race when run in parallel.
make -C build -j1 INSTALL_ROOT="$PKG" install

site=$(find "$PKG"/usr/lib -maxdepth 2 -type d -name site-packages)
for m in QtCore QtGui QtWidgets QtDBus QtNetwork QtQml QtQuick QtSvg QtOpenGL \
	QtMultimedia QtTextToSpeech QtWebChannel QtWebSockets QtPositioning \
	QtSerialPort QtSensors QtQuick3D QtRemoteObjects QtDesigner QtHelp \
	QtStateMachine QtSpatialAudio; do
	test -n "$(find "$site/PyQt6" -maxdepth 1 -name "$m.*.so")"
done
test -n "$(find "$site/dbus/mainloop" -maxdepth 1 -name 'pyqt6.*.so')"

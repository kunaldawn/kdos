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

patch -p1 -i "$PORT_SRC/sip-minimum-abi.patch"

sip-build \
	--confirm-license \
	--qmake /usr/lib/qt5/bin/qmake \
	--api-dir /usr/share/qt5/qsci/api/python \
	--disable QtBluetooth \
	--disable QtNfc \
	--disable QtLocation \
	--disable QtPositioning \
	--disable QtSensors \
	--disable QtWebChannel \
	--disable QtRemoteObjects \
	--disable QtTextToSpeech \
	--disable QtXmlPatterns \
	--disable QtQuick3D \
	--disable QtWebKit \
	--disable QtWebKitWidgets \
	--build-dir build \
	--no-make \
	--verbose
make -C build

make -C build -j1 INSTALL_ROOT="$PKG" install

site=$(find "$PKG"/usr/lib -maxdepth 2 -type d -name site-packages)
for m in QtCore QtGui QtWidgets QtDBus QtNetwork QtOpenGL QtPrintSupport \
	QtQml QtQuick QtQuickWidgets QtSvg QtMultimedia QtMultimediaWidgets \
	QtSerialPort QtWebSockets QtX11Extras QtDesigner QtHelp QtSql QtTest \
	QtXml; do
	test -n "$(find "$site/PyQt5" -maxdepth 1 -name "$m.*.so")"
done
test -n "$(find "$site/dbus/mainloop" -maxdepth 1 -name 'pyqt5.*.so')"

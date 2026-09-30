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

# PyQt5's QtCore names no minimum sip ABI, and sip 6.16.1 refuses to generate
# a module targeting ABI 12 without one; the patch declares 12.15, the ABI
# project.py already asks for, and every other module imports QtCore.
patch -p1 -i "$PORT_SRC/sip-minimum-abi.patch"

# sip-build configures one module per Qt library it finds with qmake, so the
# set built follows what is installed. The Qt 5 libraries named in depends are
# built; the modules for Qt 5 libraries that are not ported are disabled by
# name, so none can appear or vanish with build order.
#
# --api-dir writes the PyQt5.api file a Qt 5 QScintilla's Python lexer
# completes from.
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

# The generated install rules race when run in parallel.
make -C build -j1 INSTALL_ROOT="$PKG" install

site=$(find "$PKG"/usr/lib -maxdepth 2 -type d -name site-packages)
for m in QtCore QtGui QtWidgets QtDBus QtNetwork QtOpenGL QtPrintSupport \
	QtQml QtQuick QtQuickWidgets QtSvg QtMultimedia QtMultimediaWidgets \
	QtSerialPort QtWebSockets QtX11Extras QtDesigner QtHelp QtSql QtTest \
	QtXml; do
	test -n "$(find "$site/PyQt5" -maxdepth 1 -name "$m.*.so")"
done
test -n "$(find "$site/dbus/mainloop" -maxdepth 1 -name 'pyqt5.*.so')"

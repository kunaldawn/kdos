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

# KF_SKIP_PO_PROCESSING leaves every translation out: bundled data is English
# only, and without it each language's catalogue is compiled and installed.

# Qt Qml is an optional component: missing, the QML module is dropped without
# a word and QML applications fail to import it, so it is checked for.
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D KF_SKIP_PO_PROCESSING=ON \
	-D BUILD_TESTING=OFF \
	-D BUILD_QCH=OFF \
	-D BUILD_PYTHON_BINDINGS=OFF \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

[ -n "$(find "$PKG" -path '*/org/kde/notification/qmldir')" ] || {
	echo 'knotifications: the QML module was not built' >&2
	exit 1
}

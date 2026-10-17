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

cd src
/usr/lib/qt6/bin/qmake qscintilla.pro CONFIG+=release \
	QMAKE_CFLAGS_RELEASE="$CFLAGS" \
	QMAKE_CXXFLAGS_RELEASE="$CXXFLAGS" \
	QMAKE_LFLAGS_RELEASE="$LDFLAGS"
make
make INSTALL_ROOT="$PKG" install
test -e "$PKG"/usr/lib/libqscintilla2_qt6.so

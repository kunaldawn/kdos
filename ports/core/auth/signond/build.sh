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

patch -p1 -i "$PORT_SRC/qt6.patch"
qmake6 CONFIG+=release QMAKE_CFLAGS_RELEASE="$CFLAGS" QMAKE_CXXFLAGS_RELEASE="$CXXFLAGS" \
	QMAKE_LFLAGS_RELEASE="$LDFLAGS" PREFIX=/usr LIBDIR=/usr/lib \
	-after "SUBDIRS -= tests example test libsignon-qt-static.pro" "INSTALLS -= documentation"
make
make INSTALL_ROOT=$PKG install

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

patch -p1 -i "$PORT_SRC/10_install_paths.patch"
patch -p1 -i "$PORT_SRC/20_fix_rpath.patch"
patch -p1 -i "$PORT_SRC/30_multibuild.patch"

/usr/lib/qt6/bin/qmake qwt.pro CONFIG+=release \
	QMAKE_CFLAGS_RELEASE="$CFLAGS" \
	QMAKE_CXXFLAGS_RELEASE="$CXXFLAGS" \
	QMAKE_LFLAGS_RELEASE="$LDFLAGS"
make
make INSTALL_ROOT="$PKG" install

test -e "$PKG"/usr/lib/libqwt-qt6.so
test -e "$PKG"/usr/include/qwt/qwt_plot.h

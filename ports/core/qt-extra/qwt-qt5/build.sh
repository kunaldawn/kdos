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

# The qwt tarball built a second time, against Qt 5. The three Alpine
# qwtconfig patches are qwt's: install under /usr, no rpath, and the library,
# the qmake feature and the Designer plugin named or placed per Qt major
# version, so this build is libqwt-qt5 with Qt5Qwt6.pc, beside qwt's
# libqwt-qt6. The fourth puts the headers under /usr/include/qwt-qt5, the
# first directory GNU Radio's FindQwt tries; /usr/include/qwt is the Qt 6
# build's.
patch -p1 -i "$PORT_SRC/10_install_paths.patch"
patch -p1 -i "$PORT_SRC/20_fix_rpath.patch"
patch -p1 -i "$PORT_SRC/30_multibuild.patch"
patch -p1 -i "$PORT_SRC/40_headers_per_qt.patch"

/usr/lib/qt5/bin/qmake qwt.pro CONFIG+=release \
	QMAKE_CFLAGS_RELEASE="$CFLAGS" \
	QMAKE_CXXFLAGS_RELEASE="$CXXFLAGS" \
	QMAKE_LFLAGS_RELEASE="$LDFLAGS"
make
make INSTALL_ROOT="$PKG" install

# The HTML reference is the same pages the qwt port installs at this path.
rm -r "$PKG"/usr/share/doc/qwt
rmdir "$PKG"/usr/share/doc "$PKG"/usr/share

test -e "$PKG"/usr/lib/libqwt-qt5.so
test -e "$PKG"/usr/include/qwt-qt5/qwt_plot.h
test -e "$PKG"/usr/lib/pkgconfig/Qt5Qwt6.pc

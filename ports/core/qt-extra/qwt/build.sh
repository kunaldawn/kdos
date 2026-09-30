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

# Alpine's three qwtconfig patches: install under /usr rather than
# /usr/local/qwt-6.3.0, with headers in /usr/include/qwt, the qmake feature
# and the Designer plugin under Qt 6's own directories, and no rpath. The
# library is named libqwt-qt6, so qwt-qt5's libqwt-qt5 installs beside it;
# QGIS's FindQwt looks for that name first.
patch -p1 -i "$PORT_SRC/10_install_paths.patch"
patch -p1 -i "$PORT_SRC/20_fix_rpath.patch"
patch -p1 -i "$PORT_SRC/30_multibuild.patch"

/usr/lib/qt6/bin/qmake qwt.pro
make
make INSTALL_ROOT="$PKG" install

test -e "$PKG"/usr/lib/libqwt-qt6.so
test -e "$PKG"/usr/include/qwt/qwt_plot.h

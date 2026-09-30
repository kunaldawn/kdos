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

# The Qt 6 build only: libqscintilla2_qt6, its headers, the qscintilla2.prf
# qmake feature and the API files under Qt's data directory, where Octave,
# QGIS and the Python bindings (python3-qscintilla, built from this same
# tarball) find them. The Qt Designer plugin is not built. Bundled data is
# English only, so the translation catalogues are removed after install.
cd src
/usr/lib/qt6/bin/qmake qscintilla.pro CONFIG+=release \
	QMAKE_CFLAGS_RELEASE="$CFLAGS" \
	QMAKE_CXXFLAGS_RELEASE="$CXXFLAGS" \
	QMAKE_LFLAGS_RELEASE="$LDFLAGS"
make
make INSTALL_ROOT="$PKG" install
find "$PKG" -name 'qscintilla_*.qm' -delete
test -e "$PKG"/usr/lib/libqscintilla2_qt6.so

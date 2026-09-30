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

# PyQt6.Qsci, built from the QScintilla tarball's Python directory against the
# installed libqscintilla2_qt6 (the qscintilla port): with no src/ beside it,
# the project links the system library rather than compiling a second copy.
# The .sip files are installed with the module, because QGIS's own bindings
# %Import them. The API file goes where python3-pyqt6 puts PyQt6's, so an
# editor's autocompletion sees both.
cd Python
cp pyproject-qt6.toml pyproject.toml
sip-build \
	--qmake /usr/lib/qt6/bin/qmake \
	--api-dir /usr/share/qt6/qsci/api/python \
	--build-dir build \
	--no-make \
	--qmake-setting "QMAKE_CFLAGS_RELEASE = $CFLAGS" \
	--qmake-setting "QMAKE_CXXFLAGS_RELEASE = $CXXFLAGS" \
	--qmake-setting "QMAKE_LFLAGS_RELEASE = $LDFLAGS" \
	--verbose
make -C build
make -C build -j1 INSTALL_ROOT="$PKG" install

site=$(find "$PKG"/usr/lib -maxdepth 2 -type d -name site-packages)
test -n "$(find "$site/PyQt6" -maxdepth 1 -name 'Qsci.*.so')"
test -d "$site/PyQt6/bindings/Qsci"

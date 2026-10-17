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

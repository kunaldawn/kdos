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

# The language names come from iso-codes at run time, under ISO_CODES_PREFIX.
cmake -S . -B build -G Ninja \
	-D CMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-D CMAKE_BUILD_TYPE=Release \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D QT_VER=6 \
	-D ISO_CODES_PREFIX=/usr \
	-D BUILD_STATIC_LIBS=OFF \
	-D CMAKE_DISABLE_FIND_PACKAGE_Doxygen=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

# Bundled data is English only, and English is the untranslated source text.
rm -rf "$PKG/usr/share/qt6/translations"

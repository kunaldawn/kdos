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

cmake -S . -B build -G Ninja \
	-D CMAKE_BUILD_TYPE=Release \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D Clang_DIR=/usr/lib/cmake/clang \
	-D CLANG_LINK_CLANG_DYLIB=OFF \
	-D CastXML_INSTALL_MAN_DIR=share/man \
	-D SPHINX_MAN=ON \
	-D SPHINX_HTML=OFF \
	-D BUILD_TESTING=OFF
cmake --build build
DESTDIR=$PKG cmake --install build
test -x "$PKG/usr/bin/castxml"
test -f "$PKG/usr/share/castxml/clang/include/stddef.h"
test -f "$PKG/usr/share/man/man1/castxml.1"

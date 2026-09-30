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

# The API reference needs Doxygen and Graphviz and the tests need GTest; both
# are off, so neither is a dependency.
mkdir -p build && cd build
cmake .. -G Ninja \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DURIPARSER_SHARED_LIBS=ON \
	-DURIPARSER_BUILD_DOCS=OFF \
	-DURIPARSER_BUILD_TESTS=OFF \
	-DURIPARSER_BUILD_FUZZERS=OFF \
	-DURIPARSER_BUILD_TOOLS=ON \
	-DURIPARSER_BUILD_CHAR=ON \
	-DURIPARSER_BUILD_WCHAR_T=ON
ninja
DESTDIR=$PKG ninja install

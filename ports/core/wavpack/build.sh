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

# The four commands are built with iconv from musl itself; WAVPACK_ENABLE_LIBICONV
# would only prefer a separate libiconv, which this tree does not have.
# WAVPACK_INSTALL_DOCS installs the four manual pages and nothing else.
cmake -B build -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON -DBUILD_TESTING=OFF \
	-DWAVPACK_BUILD_PROGRAMS=ON \
	-DWAVPACK_ENABLE_LIBICONV=OFF \
	-DWAVPACK_ENABLE_LEGACY=ON \
	-DWAVPACK_ENABLE_DSD=ON \
	-DWAVPACK_ENABLE_THREADS=ON \
	-DWAVPACK_ENABLE_ASM=ON \
	-DWAVPACK_INSTALL_DOCS=ON \
	-DWAVPACK_INSTALL_PKGCONFIG_MODULE=ON \
	-DWAVPACK_INSTALL_CMAKE_MODULE=ON
cmake --build build
DESTDIR=$PKG cmake --install build

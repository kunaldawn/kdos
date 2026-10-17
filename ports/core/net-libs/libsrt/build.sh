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

cmake -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DCMAKE_INSTALL_BINDIR=bin \
	-DENABLE_SHARED=ON \
	-DENABLE_STATIC=ON \
	-DENABLE_APPS=ON \
	-DENABLE_CXX11=ON \
	-DENABLE_ENCRYPTION=ON \
	-DUSE_ENCLIB=openssl-evp \
	-DENABLE_BONDING=ON \
	-DENABLE_LOGGING=ON \
	-DENABLE_HEAVY_LOGGING=OFF \
	-DENABLE_TESTING=OFF \
	-DENABLE_UNITTESTS=OFF \
	-DENABLE_EXAMPLES=OFF \
	-DENABLE_CODE_COVERAGE=OFF
cmake --build build
DESTDIR=$PKG cmake --install build

test -f "$PKG/usr/lib/libsrt.so"
test -x "$PKG/usr/bin/srt-live-transmit"

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
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DCMAKE_INSTALL_LIBEXECDIR=lib \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_C_FLAGS_RELEASE="$CFLAGS -DNDEBUG" \
	-DCMAKE_CXX_FLAGS_RELEASE="$CXXFLAGS -DNDEBUG" \
	-DDEPS=LOCAL \
	-DDEP_FMT=BUNDLED \
	-DDEP_BLAKE3=BUNDLED \
	-DDEP_CPPHTTPLIB=BUNDLED \
	-DDEP_TLEXPECTED=BUNDLED \
	-DCCACHE_DEV_MODE=OFF \
	-DHTTP_STORAGE_BACKEND=ON \
	-DREDIS_STORAGE_BACKEND=OFF \
	-DENABLE_TESTING=OFF \
	-DENABLE_DOCUMENTATION=OFF \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

install -d $PKG/usr/lib/ccache
for c in gcc g++ cc c++ clang clang++; do
	ln -s /usr/bin/ccache $PKG/usr/lib/ccache/$c
done

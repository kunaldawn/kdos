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
        -DCMAKE_INSTALL_LIBDIR=/usr/lib \
        -DCMAKE_INSTALL_LIBEXECDIR=/usr/lib \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_FLAGS_RELEASE="$CFLAGS -DNDEBUG" \
        -DWITH_SYSTEM_SPNG=ON \
        -DWITH_SYSTEM_ZLIB=ON \
        -DWITH_TESTS=OFF \
        -DWITH_JPEG8=ON \
        -DWITH_SIMD=ON \
        -DREQUIRE_SIMD=ON \
        -DCMAKE_SKIP_INSTALL_RPATH=ON \
        -Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

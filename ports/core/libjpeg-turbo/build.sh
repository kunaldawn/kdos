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

# The x86-64 SIMD kernels are assembled by nasm or yasm. Without REQUIRE_SIMD
# a missing assembler is only a warning and the library ships the plain C
# paths; with it, the configure stops.
cmake -S . -B build -G Ninja \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DCMAKE_INSTALL_LIBDIR=/usr/lib \
        -DCMAKE_INSTALL_LIBEXECDIR=/usr/lib \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_FLAGS_RELEASE="$CFLAGS" \
        -DCMAKE_CXX_FLAGS_RELEASE="$CXXFLAGS" \
        -DWITH_JPEG8=ON \
        -DWITH_SIMD=ON \
        -DREQUIRE_SIMD=ON \
        -Wno-dev 
cmake --build build
DESTDIR=$PKG cmake --install build

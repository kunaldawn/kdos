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
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_BUILD_TYPE=Release \
	-D CMAKE_PREFIX_PATH=/usr \
	-D CMAKE_C_FLAGS_RELEASE="$CFLAGS" \
	-D CMAKE_CXX_FLAGS_RELEASE="$CXXFLAGS" \
	-D ZIG_TARGET_TRIPLE=native \
	-D ZIG_TARGET_MCPU=baseline \
	-D ZIG_USE_LLVM_CONFIG=ON \
	-D ZIG_STATIC_LLVM=OFF \
	-D ZIG_STATIC_ZLIB=ON \
	-D ZIG_STATIC_ZSTD=ON \
	-D ZIG_PIE=OFF \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

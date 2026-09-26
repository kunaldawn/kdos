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

mv $SRC_ROOT/cmake-${version}.src $SRC_ROOT/cmake
# The Mach-O linker includes mach-o/compact_unwind_encoding.h from
# $LLVM_MAIN_SRC_DIR/../libunwind/include, which a standalone build resolves
# beside the source tree. The header comes from the same release, so it is
# the encoding this linker was written against.
mv $SRC_ROOT/libunwind-${version}.src $SRC_ROOT/libunwind

cmake -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=$_prefix \
	-D CMAKE_BUILD_TYPE=Release \
	-D CMAKE_C_FLAGS_RELEASE="$CFLAGS" \
	-D CMAKE_CXX_FLAGS_RELEASE="$CXXFLAGS" \
	-D LLVM_DIR=$_prefix/lib/cmake/llvm \
	-D LLVM_INCLUDE_TESTS=OFF \
	-Wno-dev

cmake --build build
DESTDIR=$PKG cmake --install build

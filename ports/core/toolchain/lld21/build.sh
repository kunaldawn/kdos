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
_unwind_inc=$SRC_ROOT/libunwind-${version}.src/include

cmake -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=$_prefix \
	-D CMAKE_BUILD_TYPE=Release \
	-D CMAKE_C_FLAGS_RELEASE="$CFLAGS -DNDEBUG" \
	-D CMAKE_CXX_FLAGS_RELEASE="$CXXFLAGS -I$_unwind_inc -DNDEBUG" \
	-D LLVM_DIR=$_prefix/lib/cmake/llvm \
	-D LLVM_INCLUDE_TESTS=OFF \
	-Wno-dev

cmake --build build
DESTDIR=$PKG cmake --install build

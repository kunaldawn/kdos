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

cmake -S runtimes -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=$_prefix \
	-D CMAKE_BUILD_TYPE=Release \
	-D CMAKE_C_FLAGS_RELEASE="$CFLAGS -DNDEBUG" \
	-D CMAKE_CXX_FLAGS_RELEASE="$CXXFLAGS -DNDEBUG" \
	-D LLVM_ENABLE_RUNTIMES="libunwind" \
	-D LLVM_ENABLE_PER_TARGET_RUNTIME_DIR=OFF \
	-D LLVM_INCLUDE_TESTS=OFF \
	-D LIBUNWIND_INCLUDE_TESTS=OFF \
	-D LIBUNWIND_INSTALL_HEADERS=ON \
	-D LIBUNWIND_ENABLE_ASSERTIONS=OFF \
	-Wno-dev

cmake --build build
DESTDIR=$PKG cmake --install build

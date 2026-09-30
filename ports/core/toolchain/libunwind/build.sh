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

# A PRIVATE PREFIX, because /usr/include/libunwind.h and libunwind.so are the
# nongnu libunwind's (libunwind-nongnu): the remote-unwinding library with
# libunwind-ptrace and pkg-config files that htop, GStreamer, libcamera and
# samba link.
# The two own the same file names and only the nongnu one unwinds another
# process, so this one installs where no compiler or linker searches by
# default, and a consumer that wants it names the prefix.
cmake -S runtimes -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=$_prefix \
	-D CMAKE_BUILD_TYPE=Release \
	-D CMAKE_C_FLAGS_RELEASE="$CFLAGS" \
	-D CMAKE_CXX_FLAGS_RELEASE="$CXXFLAGS" \
	-D LLVM_ENABLE_RUNTIMES="libunwind" \
	-D LLVM_ENABLE_PER_TARGET_RUNTIME_DIR=OFF \
	-D LLVM_INCLUDE_TESTS=OFF \
	-D LIBUNWIND_INCLUDE_TESTS=OFF \
	-D LIBUNWIND_INSTALL_HEADERS=ON \
	-Wno-dev

cmake --build build
DESTDIR=$PKG cmake --install build

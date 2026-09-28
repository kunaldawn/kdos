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

# Two headers, a pkg-config file and a CMake package. zeromq installs no CMake
# package of its own, so cppzmq finds it through its bundled pkg-config module
# and installs that module beside its configuration for consumers to reuse.
# The tests fetch Catch2 at configure time and stay off.
cmake -B build -G Ninja \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DCPPZMQ_BUILD_TESTS=OFF
cmake --build build
DESTDIR=$PKG cmake --install build

test -e "$PKG"/usr/include/zmq.hpp
test -e "$PKG"/usr/lib/pkgconfig/cppzmq.pc

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

# libj1939.h declares two `struct timespec` fields and includes no <time.h>;
# glibc leaks that definition through another header and musl does not, so the
# isobusfs tools fail on an incomplete type while every other target builds.
# CFLAGS rather than CPPFLAGS: cmake reads the first into CMAKE_C_FLAGS and
# ignores the second entirely.
export CFLAGS="$CFLAGS -include time.h"

mkdir -p build && cd build
cmake .. -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr
make
make DESTDIR=$PKG install

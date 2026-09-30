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

# The kernel's msi2500 driver claims these dongles as a V4L2 SDR device;
# DETACH_KERNEL_DRIVER lets the library take a device the driver holds.
# Device access comes from /etc/udev/rules.d/70-kdos-sdr.rules.
mkdir -p build && cd build
cmake .. -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
	-DLIB_SUFFIX= \
	-DCMAKE_SKIP_INSTALL_RPATH=ON \
	-DDETACH_KERNEL_DRIVER=ON
make
make DESTDIR=$PKG install
rm "$PKG/usr/lib/libmirisdr.a"

test -f "$PKG/usr/lib/libmirisdr.so"
test -f "$PKG/usr/include/mirisdr.h"
test -x "$PKG/usr/bin/miri_sdr"

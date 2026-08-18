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

# Hidraw backend only.
#
# The hidraw backend goes through the kernel's HID interface, which
# fs/etc/udev/rules.d/70-kdos-debug.rules grants to `dialout`. The libusb
# backend instead detaches the kernel driver to claim the interface, which
# bypasses that access model entirely. Building both would let a program pick
# either at runtime, so only one is built.
mkdir -p build && cd build
cmake .. \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON \
	-DHIDAPI_WITH_LIBUSB=OFF \
	-DHIDAPI_WITH_HIDRAW=ON \
	-DHIDAPI_BUILD_HIDTEST=OFF
make
make DESTDIR=$PKG install

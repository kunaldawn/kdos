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

# IT IS ALSO THE ONLY THING THAT WRITES A FUSE BYTE, which is the operation
# that bricks a chip when it goes wrong and cannot be undone from software
# afterwards. Every Arduino IDE has this underneath it; here it is the tool
# itself, so the clock source and the reset-disable bit are things somebody
# sets deliberately rather than through a menu that does not say what it did.
#
# libserialport is what makes `-P /dev/ttyUSB0` find the right adapter among
# several, and the `dialout` rules are what let a non-root user open it.
#
# EVERY LIBRARY IS A BARE find_library WITH NO SWITCH, so the ones that are not
# ports are pinned by setting their result: a cache value that is not
# -NOTFOUND stops the search. libusb-0.1 (`usb`, libusb-compat) is not a port,
# which leaves usbtiny, micronucleus, the USB jtagmkII/Dragon/AVRISP mkII path,
# pickit2 and the flip1/flip2 DFU bootloaders out of this binary; `usb0` is
# the Windows one and `ftdi` is the pre-1.0 libftdi that libftdi1 replaces.
# libusb-1.0, hidapi, libftdi1, libelf, readline and libserialport are all
# `depends`.
mkdir -p build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DCMAKE_INSTALL_SYSCONFDIR=/etc \
	-DHAVE_LINUXGPIO=ON \
	-DHAVE_LINUXSPI=ON \
	-DHAVE_PARPORT=OFF \
	-DHAVE_LIBUSB=OFF \
	-DHAVE_LIBUSB_WIN32=OFF \
	-DHAVE_LIBFTDI=OFF \
	-DBUILD_DOC=OFF \
	-DFORCE_DISABLE_PYTHON_SUPPORT=ON
ninja
DESTDIR=$PKG ninja install
install -Dm644 ../src/elf2tag.1 -t "$PKG/usr/share/man/man1"

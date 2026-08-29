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
mkdir -p build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_SYSCONFDIR=/etc \
	-DHAVE_LINUXGPIO=ON \
	-DHAVE_LINUXSPI=ON \
	-DHAVE_PARPORT=OFF \
	-DBUILD_DOC=OFF
ninja
DESTDIR=$PKG ninja install

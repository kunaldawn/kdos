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

mkdir -p build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_SYSCONFDIR=/etc \
	-DHAVE_LINUXGPIO=ON \
	-DHAVE_LINUXSPI=ON \
	-DHAVE_PARPORT=OFF \
	-DHAVE_LIBUSB_WIN32=OFF \
	-DHAVE_LIBFTDI=OFF \
	-DBUILD_DOC=OFF \
	-DFORCE_DISABLE_PYTHON_SUPPORT=ON
ninja
DESTDIR=$PKG ninja install
install -Dm644 ../src/elf2tag.1 -t "$PKG/usr/share/man/man1"

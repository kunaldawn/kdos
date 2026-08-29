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

# iceprog PROGRAMS ONE FAMILY AND THIS PROGRAMS THE REST. icestorm ships a
# programmer for iCE40 over FTDI and nothing else; openFPGALoader knows about
# a hundred boards and half a dozen cable types, which is what makes the flow
# usable on hardware somebody actually has rather than on the one dev board
# the toolchain was written against.
#
# -DENABLE_UDEV=ON only makes it READ udev to name a device; the rules this
# tree ships in fs/etc/udev/rules.d/70-kdos-*.rules are what grant access, and
# without them the board is present, enumerated and unopenable.
mkdir -p build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DBUILD_STATIC=OFF \
	-DENABLE_UDEV=ON \
	-DENABLE_CMSISDAP=ON \
	-DENABLE_LIBGPIOD=OFF
ninja
DESTDIR=$PKG ninja install

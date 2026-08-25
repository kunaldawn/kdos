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

# libnvme HAS BEEN A PORT WITH NO CONSUMER. This is it: SMART is an ATA
# interface and says nothing about an NVMe, whose wear, media errors and
# thermal throttling live in log pages only this reads.
#
# -Dsystemd=disabled and -Ddocs-build=false: the udev rules it would otherwise
# install name systemd units, and the man pages want a python toolchain to
# render. json-c is what makes `-o json` work, which is what turns this into
# something a script can read.
meson setup build \
	--prefix=/usr \
	--sysconfdir=/etc \
	--libdir=lib \
	--buildtype=release \
	-Dsystemd=disabled \
	-Ddocs-build=false \
	-Dtests=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

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

# SMART is an ATA interface and says nothing about an NVMe, whose wear,
# media errors and thermal throttling live in log pages only this reads.
# libnvme is built from this tree and installs under the libnvme3 name.
#
# -Dnvmf-autoconnect=disabled: its udev rules, dracut snippets and
# dispatcher scripts start systemd units. -Ddocs=man with -Ddocs-build=false
# installs the pages prebuilt in the archive. -Dpython=disabled keeps the
# bindings from appearing whenever swig happens to be installed. json-c is what makes
# `-o json` work, which is what turns this into something a script can read.
meson setup build \
	--prefix=/usr \
	--sysconfdir=/etc \
	--libdir=lib \
	--buildtype=release \
	-Dnvmf-autoconnect=disabled \
	-Dpython=disabled \
	-Ddocs=man \
	-Ddocs-build=false \
	-Dtests=false \
	-Dexamples=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

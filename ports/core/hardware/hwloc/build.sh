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

./configure \
	--prefix=/usr \
	--sysconfdir=/etc \
	--libdir=/usr/lib \
	--localstatedir=/var \
	--disable-static \
	--enable-libxml2 \
	--enable-libudev \
	--enable-pci \
	--enable-cairo \
	--disable-cuda \
	--disable-nvml \
	--disable-rsmi \
	--disable-opencl \
	--disable-levelzero \
	--disable-gl \
	--disable-doxygen
make
make DESTDIR=$PKG install

rm -f "$PKG/usr/share/hwloc/hwloc-dump-hwdata.service"
desktop-file-edit --set-icon=cpu "$PKG/usr/share/applications/lstopo.desktop"

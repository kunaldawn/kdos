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

# The vendor GPU and accelerator back ends (CUDA, NVML, ROCm SMI, OpenCL,
# Level Zero, NV-CONTROL) have no library here to probe; they are switched off
# so a library that turns up later cannot change the package silently.
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

# A systemd unit for hwloc-dump-hwdata; there is no systemd to read it.
rm -f "$PKG/usr/share/hwloc/hwloc-dump-hwdata.service"

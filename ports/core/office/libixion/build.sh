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

# Boost 1.89 made Boost.System header-only and stopped shipping its library,
# which the bundled boost.m4 still looks for; the patch is upstream's update of
# boost.m4, and configure is regenerated from it.
patch -p1 -i "$PORT_SRC/boost-1.89.patch"
autoreconf -fi

# The Vulkan compute engine is experimental and off. The python module is
# built; it installs into site-packages beside the library.
./configure --prefix=/usr --libdir=/usr/lib --disable-static \
	--with-boost=/usr \
	--enable-threads \
	--enable-python \
	--disable-vulkan \
	--disable-debug-utils
make
make DESTDIR=$PKG install

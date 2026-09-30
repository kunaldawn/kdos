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

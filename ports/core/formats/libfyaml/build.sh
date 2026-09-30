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

# libyaml is the fy-tool --compat mode's reference parser. libclang is the
# optional C reflection support, which nothing here uses; left to its default
# it is found through any llvm-config in the chroot and links libclang in.
# The manual pages ship from doc/canned-man: the Sphinx route needs pip3
# metadata for four themes and extensions this tree does not install.
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--disable-static \
	--without-libclang \
	--disable-network
make
make DESTDIR=$PKG install

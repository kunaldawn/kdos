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

# ntfs-3g lets wimlib capture an NTFS volume and apply an image to one with
# its security descriptors and alternate data streams; FUSE gives
# `wimlib-imagex mount`. Both are named so a missing library fails configure.
./configure --prefix=/usr --disable-static \
	--with-ntfs-3g --with-fuse
make
make DESTDIR=$PKG install

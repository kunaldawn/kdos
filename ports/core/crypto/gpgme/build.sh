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

# The getdents fd-closing path declares its dirent64 with ino64_t and off64_t,
# which musl exposes only under _LARGEFILE64_SOURCE; off, gpgme closes a
# child's descriptors through the portable loop instead.
./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--disable-linux-getdents \
	--disable-gpg-test \
	--disable-gpgsm-test \
	--disable-g13-test \
	--disable-static

make
make DESTDIR=$PKG install

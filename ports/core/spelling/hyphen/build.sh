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

# hunspell-en owns /usr/share/hyphen. The pattern file in this tarball is an
# older copy of the one it installs, so hyph_DATA is emptied for both the
# build and the install; left set, the two packages collide on
# hyph_en_US.dic, and the build regenerates it with patch and awk.
./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--disable-static
make hyph_DATA=
make DESTDIR=$PKG install hyph_DATA=

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

# --without-qt: the qphotorec GUI is the one part of this that would put Qt on
# the host, and the recovery tools are all ncurses. The three library flags are
# what make the difference between reading a damaged ext4 and guessing at it.
./configure \
	--prefix=/usr \
	--without-qt \
	--enable-sudo=no
make
make DESTDIR=$PKG install

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

# ITS `dumb` TERMINAL IS WHY THIS FITS HERE. gnuplot draws into a character
# grid — the same surface every other program on this desktop draws into — so a
# plot is something you get at a prompt over ssh, on tty1, or inside foot with
# no window and no toolkit. `sixel` is the richer version of the same thing and
# foot supports it.
#
# --without-qt and no wxWidgets: the hard rule. What it costs is the
# interactive zoom-and-pan window, and `set term pngcairo` plus an image viewer
# covers the case where somebody wants pixels.
./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--with-readline=gnu \
	--without-qt \
	--without-wx \
	--without-x \
	--with-bitmap-terminals \
	--enable-history-file
make
make DESTDIR=$PKG install

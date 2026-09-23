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
# no window and no toolkit. `sixelgd` is the richer version of the same thing,
# foot supports it, and it exists only with libgd — as do the png, jpeg and
# animated gif terminals.
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
	--with-gd \
	--with-cairo \
	--with-lua \
	--without-libcerf \
	--without-amos \
	--without-caca \
	--without-latex \
	--enable-history-file
make
make DESTDIR=$PKG install

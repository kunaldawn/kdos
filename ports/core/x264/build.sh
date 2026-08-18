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

# The licence is the point of this comment.
#
# x264 is GPL-2-or-later, and ffmpeg links it only under --enable-gpl, which
# makes the resulting ffmpeg binary GPL-2+ rather than LGPL. That consequence
# is recorded in LICENSE.notice beside ports/core/ffmpeg, which is where a
# redistributor is expected to find it.
#
# x264 has no versioned releases; upstream ships snapshots. `_commit` pins the
# one this recipe builds so the tarball is reproducible.
./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--enable-shared \
	--enable-pic \
	--disable-cli \
	--disable-opencl
make
make DESTDIR=$PKG install

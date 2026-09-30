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

# Every codec is named, because configure drops a missing one without a word:
# zlib for compressed snapshots and ZIP archives, libbz2 for bzip2-packed
# tapes, libgcrypt for the signatures in RZX recordings, audiofile for WAV
# tapes. The real GLib, not the bundled stand-in: fuse's front ends link it
# too, and two copies of one API in one process is a clash waiting for a
# symbol.
./configure --prefix=/usr --libdir=/usr/lib --disable-static \
	--with-zlib \
	--with-bzip2 \
	--with-libgcrypt \
	--with-libaudiofile \
	--with-wav-backend=audiofile \
	--without-fake-glib
make
make DESTDIR=$PKG install

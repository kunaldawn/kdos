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

# An audio CD has no error correction worth trusting, so a plain read returns
# clicks and dropouts where the disc is scratched. Paranoia reads each sector
# more than once and aligns the overlaps. mpv's cdda:// and mpd's cdio_paranoia
# input read through it, and cd-paranoia is the ripper.
#
# -std=gnu17: the getopt.h cd-paranoia carries declares `int getopt ()`, which
# C23 reads as a function taking no arguments and which then conflicts with the
# C library's declaration.
export CFLAGS="$CFLAGS -std=gnu17"
./configure --prefix=/usr --libdir=/usr/lib --disable-static \
	--disable-example-progs \
	--disable-cpp-progs
make
make DESTDIR=$PKG install

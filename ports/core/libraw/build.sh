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

autoreconf -f -i

# A RAW FILE IS THE ONLY COPY AND NOTHING ELSE HERE READS IT. Every camera
# above the cheapest writes CR3, NEF, ARW or DNG, and the JPEG beside it is a
# lossy preview the camera made — so an archive of raws with no decoder is an
# archive nobody can open in ten years. libraw is what imagemagick, exiv2's
# preview extraction and any thumbnailer call to get pixels out of one.
#
# --disable-examples: dcraw_emu and friends are demonstration programs, and
# the library is what consumers link. --enable-openmp is off for the
# oversubscription reason OpenBLAS's recipe states.
./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--disable-static \
	--disable-examples \
	--disable-openmp \
	--enable-jpeg \
	--enable-lcms
make
make DESTDIR=$PKG install

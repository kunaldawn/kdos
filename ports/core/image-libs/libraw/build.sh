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

export CFLAGS="${CFLAGS/-O2/-O3}" CXXFLAGS="${CXXFLAGS/-O2/-O3}"

# A RAW FILE IS THE ONLY COPY AND NOTHING ELSE HERE READS IT. Every camera
# above the cheapest writes CR3, NEF, ARW or DNG, and the JPEG beside it is a
# lossy preview the camera made — so an archive of raws with no decoder is an
# archive nobody can open in ten years. libraw is what imagemagick, exiv2's
# preview extraction and any thumbnailer call to get pixels out of one.
#
# --disable-examples: dcraw_emu and friends are demonstration programs, and
# the library is what consumers link. --enable-openmp parallelises the
# demosaic and the lossless decoders over gcc's libgomp; the probe drops it
# in silence when the compiler refuses -fopenmp, so the check after the
# install proves it is on, through the flag it adds to libraw.pc.
./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--disable-static \
	--disable-examples \
	--enable-openmp \
	--enable-jpeg \
	--enable-lcms \
	--enable-zlib
make
make DESTDIR=$PKG install
grep -q -- -fopenmp "$PKG"/usr/lib/pkgconfig/libraw.pc

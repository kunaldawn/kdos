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

# EVERY IMAGE FORMAT IS ON DELIBERATELY. leptonica answers a missing library by
# dropping that format silently, and the format it drops is usually the one a
# scanner produced — multi-page TIFF, which is what every document feeder in
# existence writes. A leptonica without libtiff makes tesseract report an
# unreadable file rather than an unsupported one.
./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--disable-static \
	--with-libpng \
	--with-jpeg \
	--with-libtiff \
	--with-libwebp \
	--with-libopenjpeg \
	--with-zlib
make
make DESTDIR=$PKG install

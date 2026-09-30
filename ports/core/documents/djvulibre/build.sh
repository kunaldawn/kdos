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

patch -p1 -i "$PORT_SRC/djvuport-stack-overflow.patch"

# JPEG and TIFF are link probes that build without the library when it is
# absent: without JPEG ddjvu cannot decode a photo layer and c44 cannot read
# one. rsvg-convert rasterises the MIME icons from djvu.svg; without it the
# prebuilt PNGs in the tarball are copied instead.
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--disable-static \
	--with-jpeg \
	--with-tiff \
	--enable-xmltools \
	--enable-desktopfiles
make
make DESTDIR=$PKG install

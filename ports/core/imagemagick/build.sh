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

# Four delegates, each a format or a facility with no other route on this
# host:
#
#   --with-lcms     the ICC engine. Without it `-profile` is accepted and
#                   silently ignored, which is worse than refused.
#   --with-heic     HEIC and AVIF — what a phone camera writes.
#   --with-openjp2  JPEG 2000 — what scanners and archives write.
#   --with-openexr  high dynamic range — what a renderer writes.
#
# Each needs its port present at configure time; autotools answers a missing
# one by disabling the feature rather than failing, so dropping one of these
# from `depends` produces a build that succeeds and quietly cannot read the
# format.

./configure \
	--prefix=/usr \
	--sysconfdir=/etc \
	--mandir=/usr/share/man \
	--without-modules \
	--with-threads \
	--with-png \
	--with-jpeg \
	--with-tiff \
	--with-webp \
	--with-zlib \
	--with-xml \
	--with-freetype \
	--with-fontconfig \
	--with-pango \
	--without-x \
	--without-perl \
	--without-fftw \
	--without-djvu \
	--without-fpx \
	--without-gslib \
	--without-gvc \
	--with-heic \
	--without-jxl \
	--with-lcms \
	--without-lqr \
	--with-openexr \
	--with-openjp2 \
	--without-raqm \
	--without-raw \
	--without-rsvg \
	--without-wmf
make
make DESTDIR=$PKG install

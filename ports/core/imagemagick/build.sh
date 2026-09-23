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

# Every delegate is named, on or off. autotools answers a missing library by
# disabling the feature rather than failing, so a delegate left to detection
# produces a build that succeeds and quietly cannot read the format — and one
# whose port happens to be built earlier gets it anyway, so the result follows
# build order. The ones worth naming:
#
#   --with-lcms     the ICC engine. Without it `-profile` is accepted and
#                   silently ignored, which is worse than refused.
#   --with-heic     HEIC and AVIF — what a phone camera writes.
#   --with-jxl      JPEG XL.
#   --with-raw      camera RAW (CR2, NEF, ARW, DNG) through libraw.
#   --with-openjp2  JPEG 2000 — what scanners and archives write.
#   --with-openexr  high dynamic range — what a renderer writes.
#   --with-rsvg     SVG through librsvg; the internal MSVG renderer gets
#                   gradients, text and clipping wrong.
#   --with-raqm     shaped, bidirectional text for label:, caption: and
#                   -annotate; without it Arabic and Indic text come out as
#                   unjoined glyphs.
#   --with-fftw     the -fft and -ift operators.
#
# --without-djvu, --without-lqr and --without-uhdr: djvulibre, liblqr and
# libultrahdr are not ports. --without-dmr: MagickCache is not a port.
#
# --with-dejavu-font-dir names where ttf-dejavu installs, so type-dejavu.xml
# gives the Sans/Serif/Mono names a font without a fontconfig lookup.
#
# --without-gcc-arch: configure otherwise reads the build machine's CPUID and
# appends a -mtune for it, so the binary would be tuned for whichever machine
# built it rather than for the baseline the rest of the tree compiles to.
#
# ac_cv_path_PSDelegate=gs: configure otherwise looks for gs on the build
# machine and, finding it, writes an absolute path into delegates.xml and
# adds type-ghostscript.xml pointing at /usr/share/ghostscript/fonts/, which
# the ghostscript port does not install. Pinned to the bare name, delegates
# run gs from PATH and no font table names files that are not there, whether
# or not ghostscript was built first.

./configure \
	ac_cv_path_PSDelegate=gs \
	--prefix=/usr \
	--sysconfdir=/etc \
	--mandir=/usr/share/man \
	--without-gcc-arch \
	--without-modules \
	--with-threads \
	--with-png \
	--with-jpeg \
	--with-tiff \
	--with-webp \
	--with-zlib \
	--with-bzlib \
	--with-lzma \
	--with-zstd \
	--with-zip \
	--with-xml \
	--with-freetype \
	--with-fontconfig \
	--with-pango \
	--with-dejavu-font-dir=/usr/share/fonts/TTF \
	--without-x \
	--without-perl \
	--with-fftw \
	--without-djvu \
	--without-dmr \
	--without-fpx \
	--without-gslib \
	--without-gvc \
	--with-heic \
	--without-jbig \
	--with-jxl \
	--with-lcms \
	--without-lqr \
	--with-openexr \
	--with-openjp2 \
	--with-raqm \
	--with-raw \
	--with-rsvg \
	--without-uhdr \
	--without-wmf
make
make DESTDIR=$PKG install

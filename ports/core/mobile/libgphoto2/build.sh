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

./configure --prefix=/usr --libdir=/usr/lib --disable-static \
	--with-camlibs=all --disable-rpath \
	--enable-nls \
	--disable-lockdev --disable-ttylock \
	--with-libusb=no \
	DOXYGEN=no DOT=no
for _h in HAVE_LIBEXIF HAVE_LIBJPEG HAVE_LIBXML2 HAVE_LIBCURL HAVE_LIBGD HAVE_LIBTIFF; do
	grep -q "^#define $_h 1" config.h || { echo "libgphoto2: $_h missing" >&2; exit 1; }
done
grep -q "^#define HAVE_LIBUSB1 1" libgphoto2_port/config.h \
	|| { echo "libgphoto2: HAVE_LIBUSB1 missing" >&2; exit 1; }
make
make DESTDIR=$PKG install

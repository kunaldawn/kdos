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

# THE LIBRARY CHECKS HAVE NO FORCED-ON SPELLING. A --with-<lib> value other
# than "no" or "auto" is read as an install prefix, and the prefix path skips
# pkg-config and links -l<pkg-config name>: -lxml-2.0, -ltiff-4 and -lgdlib do
# not exist. So they stay on auto and config.h is checked instead, which turns
# a dependency missing at build time into a failed build rather than a
# libgphoto2 without that support.
#
# libxml2 feeds ptp2's Olympus Wi-Fi support and the Lumix camlib, curl the
# Lumix camlib, libgd the picture-frame camlibs' image conversion, libtiff the
# jd11 camlib's raw DNG output, libexif thumbnails and EXIF dates.
# lockdev and ttylock are serial-port lock libraries this tree does not have;
# the doxygen reference is off by naming its program "no".
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

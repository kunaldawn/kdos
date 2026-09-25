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

# EVERY GUI FRONT END IS OFF. zbar ships GTK, Qt, Java and Python bindings and
# an X overlay; the host has none of those by rule, and what is wanted here is
# zbarimg and zbarcam — a file or a camera in, text out. `--with-x=no` matters
# even with the toolkits off, or configure links the X overlay for zbarcam;
# --without-xshm and --without-xv stop the X extension probes, which link
# libXext and libXv whenever their headers are present.
#
# LIBV4L2 IS FORCED ON. zbarcam reads a camera through libv4l2 when configure
# finds libv4l2.h, which converts the pixel formats many webcams deliver and
# zbar cannot read raw. There is no switch for it, so the header's cache
# variable is preset and PKG_CHECK_MODULES then fails configure if v4l-utils
# is missing, instead of shipping a zbarcam that cannot see those cameras.
#
# --without-graphicsmagick IS WHAT MAKES --with-imagemagick BINDING: while the
# GraphicsMagick fallback is left at "check", a MagickWand that pkg-config does
# not find turns image scanning off with a notice and zbarimg is not built.
# --without-gir: the introspection data describes the GTK widget, which is off.
#
# XMLTO IS NAMED, NOT SEARCHED FOR: --enable-doc only asks, and a missing xmlto
# drops the man pages with no message. Preset, the program is run at build
# time and its absence stops the build.
#
# xmlto AND docbook-xsl: the man pages are built from docbook, and the
# stylesheet is named by its sourceforge URL. Only docbook-xsl's XML catalog
# rewrites that to the local copy — without it xsltproc tries to FETCH it and
# the build dies inside xmlto with an unresolved external entity. The catalog
# has to be NAMED: libxml2's compiled-in default is not what this build sees.
export XML_CATALOG_FILES=/etc/xml/catalog

./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--disable-static \
	--without-gtk \
	--without-qt \
	--without-java \
	--without-python \
	--without-x \
	--without-xshm \
	--without-xv \
	--enable-video \
	--with-imagemagick \
	--without-graphicsmagick \
	--without-gir \
	--with-jpeg \
	--with-dbus \
	--enable-doc \
	--enable-nls \
	XMLTO=xmlto \
	ac_cv_header_libv4l2_h=yes
make
make DESTDIR=$PKG install

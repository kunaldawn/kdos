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

export CFLAGS="$CFLAGS -include sys/types.h"

ac_cv_func_select=yes \
./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--disable-static \
	--without-cxx-binding \
	--without-python-binding \
	--without-tcl-binding \
	--without-perl-binding \
	--without-lua-binding \
	--with-libusb \
	--with-readline \
	--with-xml-support \
	--without-indi \
	--disable-html-matrix \
	--disable-pytest
grep -q '^#define HAVE_SELECT 1' include/hamlib/config.h
make
make DESTDIR=$PKG install

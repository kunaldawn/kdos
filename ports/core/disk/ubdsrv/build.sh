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

patch -p1 -i "$PORT_SRC/musl-prctl.patch"
autoreconf -fi
export CXXFLAGS="$CXXFLAGS -fpermissive"
./configure --prefix=/usr --libdir=/usr/lib --sbindir=/usr/sbin --disable-static \
	--without-gnutls \
	--with-libnfs \
	--without-libiscsi
make noinst_PROGRAMS=
make noinst_PROGRAMS= DESTDIR=$PKG install

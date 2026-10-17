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

patch -p1 -i "$PORT_SRC/werrorformat.patch"
patch -p1 -i "$PORT_SRC/wvsummary-gsf-include.patch"

export CFLAGS="$CFLAGS -Werror=int-conversion"
MKDIR_P="mkdir -p" \
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--mandir=/usr/share/man \
	--disable-static \
	--with-zlib \
	--with-png \
	--without-libwmf
make
make DESTDIR=$PKG install

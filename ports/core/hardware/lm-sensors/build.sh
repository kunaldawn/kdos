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

patch -p1 -i "$PORT_SRC/iconv-probe.patch"
make PREFIX=/usr LIBDIR=/usr/lib ETCDIR=/etc MANDIR=/usr/share/man PROG_EXTRA= BUILD_STATIC_LIB=0 LIBICONV= EXLDFLAGS="$LDFLAGS"
make PREFIX=/usr LIBDIR=/usr/lib ETCDIR=/etc MANDIR=/usr/share/man PROG_EXTRA= BUILD_STATIC_LIB=0 LIBICONV= EXLDFLAGS="$LDFLAGS" DESTDIR=$PKG install
rm -f "$PKG/usr/sbin/sensors-detect" \
	"$PKG/usr/share/man/man8/sensors-detect.8" \
	"$PKG/usr/share/zsh/site-functions/_sensors-detect"

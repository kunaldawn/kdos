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

make CFLAGS="$CFLAGS" \
	VCPPFLAGS="-DPKGBUILD='\"$(date -u -d "@$SOURCE_DATE_EPOCH" +%F)\"' -DPKGVERSION='\"keyutils-\$(VERSION)\"' -DAPIVERSION='\"libkeyutils-\$(APIVERSION)\"'"
make install DESTDIR=$PKG USRLIBDIR=/usr/lib LIBDIR=/usr/lib

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

# The Makefile stamps today's date into libkeyutils as PKGBUILD; a command-line
# VCPPFLAGS replaces all three of its definitions, so it restates the version
# macros and dates the build from SOURCE_DATE_EPOCH, or no two builds match.
make CFLAGS="$CFLAGS" \
	VCPPFLAGS="-DPKGBUILD='\"$(date -u -d "@$SOURCE_DATE_EPOCH" +%F)\"' -DPKGVERSION='\"keyutils-\$(VERSION)\"' -DAPIVERSION='\"libkeyutils-\$(APIVERSION)\"'"
make install DESTDIR=$PKG USRLIBDIR=/usr/lib LIBDIR=/usr/lib

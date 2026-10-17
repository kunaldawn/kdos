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

export CFLAGS="$CFLAGS -include fcntl.h"
meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	--wrap-mode=nodownload \
	-Dcrypto=libgcrypt \
	-Dintrospection=true \
	-Dvapi=true \
	-Dmanpage=true \
	-Dgtk_doc=false \
	-Dbash_completion=enabled \
	-Dtpm2=false \
	-Dpam=false \
	-Dtest_setup=disabled \
	-Ddebugging=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

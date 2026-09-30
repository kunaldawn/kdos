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

# sxpm and cxpm are built only when libXt and libXext are found, which is why
# both are in depends.
# --with-localedir=no turns gettext off. configure finds musl's passthrough
# gettext in libc while the gettext port's libintl.h renames every call to
# libintl_gettext, so cxpm fails to link; the library ships no catalogues.
./configure \
	--prefix=/usr \
	--sysconfdir=/etc \
	--libdir=/usr/lib \
	--disable-static \
	--disable-unit-tests \
	--with-localedir=no
make
make DESTDIR=$PKG install

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

meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib --libexecdir=/usr/lib \
	--buildtype=release \
	-Dintrospection=enabled \
	-Ddoc=disabled \
	-Dexamples=disabled \
	-Dtests=disabled \
	-Dptp-helper=disabled \
	-Dlibdw=enabled \
	-Dlibunwind=enabled \
	-Dbash-completion=enabled \
	-Dnls=disabled \
	-Dgst_debug=true
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

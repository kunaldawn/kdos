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
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	--libexecdir=/usr/lib \
	--buildtype=release \
	-Dgnutls=enabled \
	-Dopenssl=disabled \
	-Denvironment_proxy=enabled \
	-Dlibproxy=disabled \
	-Dgnome_proxy=disabled \
	-Dtests=false \
	-Dinstalled_tests=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

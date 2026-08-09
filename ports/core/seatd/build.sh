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

export CFLAGS="$CFLAGS -Uunix"
meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	-Dlibseat-builtin=enabled \
	-Dlibseat-logind=disabled \
	-Dserver=enabled \
	-Dexamples=disabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

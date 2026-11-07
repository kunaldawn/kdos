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

patch -p1 -i "$PORT_SRC/rundir-mounts.patch"

meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	--libexecdir=/usr/lib \
	--buildtype=release \
	-Ddinit=disabled \
	-Drunit=disabled \
	-Dmanage_rundir=true \
	-Dstatedir=lib/elogind \
	-Dlibrary=disabled \
	-Dman=true
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

rm "$PKG/etc/turnstile/turnstiled.conf"
rmdir "$PKG/etc/turnstile"

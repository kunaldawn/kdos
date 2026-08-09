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
	--buildtype=release \
	-Dxvfb=false \
	-Dsecure-rpc=false \
	-Dxwayland_ei=false \
	-Dlibdecor=false \
	-Dsystemd_notify=false \
	-Dglamor=true \
	-Ddri3=true \
	-Dglx=false \
	-Dxkb_dir=/usr/share/X11/xkb \
	-Dxkb_output_dir=/var/lib/xkb \
	-Dxkb_bin_dir=/usr/bin \
	-Ddocs=false \
	-Ddevel-docs=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

install -dm755 $PKG/var/lib/xkb

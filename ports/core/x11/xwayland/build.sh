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

# glx is the server side of GLX: without it an X11 client's glXChooseVisual
# finds no GLX extension and the program exits. It needs mesa's dri.pc and
# libglvnd's gl.pc.
meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	-Dxvfb=false \
	-Dsecure-rpc=false \
	-Dxwayland_ei=false \
	-Dlibdecor=false \
	-Dsystemd_notify=false \
	-Dxselinux=false \
	-Dglamor=true \
	-Ddri3=true \
	-Dglx=true \
	-Dsha1=libcrypto \
	-Dxkb_dir=/usr/share/X11/xkb \
	-Dxkb_output_dir=/var/lib/xkb \
	-Dxkb_bin_dir=/usr/bin \
	-Ddefault_font_path=/usr/share/fonts/misc/,/usr/share/fonts/75dpi/ \
	-Ddocs=false \
	-Ddevel-docs=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

install -dm755 $PKG/var/lib/xkb

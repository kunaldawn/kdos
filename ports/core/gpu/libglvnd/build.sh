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

meson setup build --buildtype=release \
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	-D x11=enabled \
	-D glx=enabled \
	-D gles1=false \
	-D egl=true \
	-D tls=false \
	-D hgl=false \
	-D asm=enabled \
	-D entrypoint-patching=enabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

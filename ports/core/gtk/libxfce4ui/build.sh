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

./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib --disable-static \
	--enable-x11 \
	--enable-wayland \
	--enable-libsm \
	--disable-startup-notification \
	--disable-glibtop \
	--disable-epoxy \
	--disable-gudev \
	--disable-gladeui2 \
	--disable-keyboard-library \
	--enable-introspection=no \
	--enable-vala=no \
	--disable-gtk-doc \
	--disable-tests
make
make DESTDIR=$PKG install

rm -f "$PKG/usr/share/applications/xfce4-about.desktop"

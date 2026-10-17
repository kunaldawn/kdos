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

export CFLAGS="$CFLAGS -DATTRIBUTE_UNUSED=__attribute__((__unused__))"
meson setup build --prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	-Denable-introspection=enabled \
	-Denable-vala=enabled \
	-Denable-gtk-doc=false \
	-Denable-tests=false \
	-Dlibsoup-abi=3.0 \
	-Dwith-pci-ids-path=/usr/share/hwdata/pci.ids \
	-Dwith-usb-ids-path=/usr/share/hwdata/usb.ids
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

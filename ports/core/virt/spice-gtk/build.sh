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

meson setup build --prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	-Dgtk=enabled \
	-Dwayland-protocols=enabled \
	-Degl=enabled \
	-Dwebdav=enabled \
	-Dbuiltin-mjpeg=true \
	-Dusbredir=enabled \
	-Dpolkit=disabled \
	-Dlibcap-ng=disabled \
	-Dusb-ids-path=/usr/share/hwdata/usb.ids \
	-Dcoroutine=gthread \
	-Dintrospection=enabled \
	-Dvapi=enabled \
	-Dlz4=enabled \
	-Dsasl=enabled \
	-Dopus=enabled \
	-Dsmartcard=enabled \
	-Dgtk_doc=disabled \
	-Dspice-common:manual=false \
	-Dspice-common:tests=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

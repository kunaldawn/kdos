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

meson setup build --prefix=/usr --libdir=lib \
	--buildtype=release \
	-Drelease=true \
	-Ddocs=false \
	-Dtests=false \
	-Dintrospection=false \
	-Dvapi=false \
	-Dconfig-env=true \
	-Dconfig-kde=true \
	-Dconfig-gnome=false \
	-Dconfig-sysconfig=false \
	-Dconfig-xdp=false \
	-Dconfig-osx=false \
	-Dconfig-windows=false \
	-Dpacrunner-duktape=true \
	-Dcurl=true
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

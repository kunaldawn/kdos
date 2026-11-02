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

meson setup build --prefix=/usr --sysconfdir=/etc --localstatedir=/var --libdir=lib \
	--buildtype=release \
	-Dsystemd=false \
	-Ddaemon=true \
	-Ddaemon_user=colord \
	-Dudev_rules=true \
	-Dbash_completion=true \
	-Dintrospection=true \
	-Dvapi=true \
	-Dsane=false \
	-Dargyllcms_sensor=false \
	-Dprint_profiles=false \
	-Dlibcolordcompat=false \
	-Dsession_example=false \
	-Dman=true \
	-Ddocs=false \
	-Dtests=false \
	-Dinstalled_tests=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

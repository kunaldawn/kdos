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

patch -p1 -i "$PORT_SRC/meson-foreach-dict.patch"
meson setup build --prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	-Dudev_rules=enabled \
	-Dudev_rules_dir=/usr/lib/udev/rules.d \
	-Dudev_hwdb=enabled \
	-Dudev_hwdb_dir=/usr/lib/udev/hwdb.d \
	-Dgtk-examples=false \
	-Dintrospection=false \
	-Ddoc=false \
	-Dinstalled-tests=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

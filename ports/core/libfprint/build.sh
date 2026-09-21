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


# THE DRIVER SET IS UPSTREAM'S DEFAULT. Narrowing it is a bet on which sensor
# a machine turns out to have, and losing that bet is silent — the reader is
# simply not detected, which reads as broken hardware.
#
# udev RULES ARE WHAT MAKE THE DEVICE REACHABLE by anything but root, and the
# HWDB IS ENABLED RATHER THAN LEFT ON `auto` — upstream's own description says
# it is "included in systemd v248 and later", which is the one place this
# image will never get it from. Without it a reader is not autosuspended and
# holds the USB bus awake, which is a laptop that does not reach its deeper
# idle states.
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

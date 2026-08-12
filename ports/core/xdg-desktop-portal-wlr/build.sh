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

# sd-bus comes from basu. The default is `auto`, which searches libsystemd
# first — on a machine that has none of the three, an auto search reports the
# failure as a missing dependency rather than as the choice it is.
#
# --libexecdir=/usr/lib matches the main portal's own layout, so both daemons
# live in one place and kdos-desktop-start has one path to look at.
meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib --libexecdir=/usr/lib \
	--buildtype=release \
	-Dsd-bus-provider=basu \
	-Dsystemd=disabled \
	-Dman-pages=disabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

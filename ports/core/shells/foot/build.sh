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

# utmp-backend=none: on Linux upstream's default execs a libutempter helper at
# /usr/lib/utempter/utempter for every window, which no port installs.
export CFLAGS="${CFLAGS/-O2/-O3} -fno-strict-aliasing"
meson setup build \
	--prefix=/usr --libdir=lib \
	--buildtype=release \
	-Dgrapheme-clustering=enabled \
	-Dterminfo=enabled \
	-Dterminfo-base-name=foot-extra \
	-Dime=true \
	-Ddocs=enabled \
	-Dutmp-backend=none \
	-Dtests=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

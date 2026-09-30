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
	-Dmaintainer-mode=false \
	-Dwarnings=min \
	-Dbuild-deprecated-api=true \
	-Dbuild-documentation=false \
	-Dvalidation=false \
	-Dbuild-pdf=false \
	-Dbuild-examples=false \
	-Dbuild-tests=false \
	-Dbenchmark=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

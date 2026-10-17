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

meson setup build --buildtype=release --prefix=/usr --sysconfdir=/etc --libdir=lib \
	-Dintrospection=enabled \
	-Dxft=enabled \
	-Dlibthai=enabled \
	-Dcairo=enabled \
	-Dfontconfig=enabled \
	-Dfreetype=enabled \
	-Dsysprof=disabled \
	-Dbuild-testsuite=false \
	-Dbuild-examples=false \
	-Dman-pages=true
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

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
	--buildtype=release -Ddefault_library=shared \
	-Djack=disabled -Dalsa=enabled -Dpulse=enabled -Doss=false \
	-Dcore=disabled -Ddsound=disabled -Dasio=disabled -Dwasapi=disabled \
	-Ddocs=false -Dinstall_docs=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

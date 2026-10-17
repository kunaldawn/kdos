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

meson setup build \
	--prefix=/usr \
	--sysconfdir=/etc \
	--libdir=lib \
	--buildtype=release \
	--wrap-mode=nodownload \
	-Djson-c=enabled \
	-Dlibkmod=enabled \
	-Dopenssl=enabled \
	-Dkeyutils=enabled \
	-Dnvmf-autoconnect=disabled \
	-Dpython=disabled \
	-Ddocs=man \
	-Ddocs-build=false \
	-Dtests=false \
	-Dexamples=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

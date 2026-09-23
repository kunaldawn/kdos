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


export CFLAGS="$CFLAGS -D_GNU_SOURCE -include sys/types.h"

meson setup build \
	--prefix=/usr --libdir=lib --sysconfdir=/etc \
	--buildtype=release \
	-Degl=enabled \
	-Dgles1=disabled \
	-Dgles2=enabled \
	-Dwayland=enabled \
	-Dvulkan=enabled \
	-Dx11=disabled \
	-Dglut=disabled \
	-Dosmesa=disabled \
	-Dlibdrm=enabled \
	-Dwith-system-data-files=true
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

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

patch -p1 -i $PORT_SRC/gl-disabler.patch
patch -p1 -i $PORT_SRC/util-without-glu.patch

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
	-Dlibdrm=disabled \
	-Dwith-system-data-files=true
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

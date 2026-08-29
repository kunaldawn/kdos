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

# -licuuc: libicuio.pc does not propagate icu-uc, so ubrk_* is unresolved.
export LDFLAGS="$LDFLAGS -licuuc"
meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	-Dwrap_mode=nodownload \
	-Dwindows=wayland \
	-Dunicode=icu \
	-Dman=disabled \
	-Dtest=disabled \
	-Dlibpng=enabled \
	-Dlibjpeg=enabled \
	-Dlibtiff=enabled \
	-Dlibrsvg=enabled \
	-Dlibnsgif=enabled \
	-Dfreeimage=disabled \
	-Dlibheif=enabled \
	-Dlibjxl=disabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

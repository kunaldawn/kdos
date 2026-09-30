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
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	-Db_ndebug=if-release \
	--wrap-mode=nodownload \
	-Dintrospection=enabled \
	-Dvapi=true \
	-Dgtk_doc=false \
	-Dtests=false \
	-Dexamples=false \
	-Dglade_catalog=disabled \
	-Dprofiling=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

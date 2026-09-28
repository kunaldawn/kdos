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
	-Dgobject_types=true \
	-Dintrospection=enabled \
	-Dgcc_vector=true \
	-Dsse2=true \
	-Darm_neon=false \
	-Dgtk_doc=false \
	-Dtests=false \
	-Dinstalled_tests=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

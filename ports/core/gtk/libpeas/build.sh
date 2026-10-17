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
	-Dpython3=true \
	-Dpython2=false \
	-Dlua51=false \
	-Dintrospection=true \
	-Dvapi=true \
	-Dwidgetry=true \
	-Dglade_catalog=false \
	-Ddemos=false \
	-Dgtk_doc=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

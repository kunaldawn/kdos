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

# The GTK 3 and GTK 4 parent-window helpers are built; the Qt ones are not,
# since Qt applications reach the portals through Qt's own platform code and
# a Qt backend here would put Qt into the build of every GTK consumer.
meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	--wrap-mode=nodownload \
	-Dbackend-gtk3=enabled \
	-Dbackend-gtk4=enabled \
	-Dbackend-qt5=disabled \
	-Dbackend-qt6=disabled \
	-Dintrospection=true \
	-Dvapi=true \
	-Ddocs=false \
	-Dtests=false \
	-Dportal-tests=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

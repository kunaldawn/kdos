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
	--prefix=/usr --sysconfdir=/etc --libdir=lib --libexecdir=/usr/lib \
	--buildtype=release \
	-Duse_systemd=false \
	-Ddefault_bus=dbus-daemon \
	-Ddbus_daemon=/usr/bin/dbus-daemon \
	-Dgtk2_atk_adaptor=false \
	-Dx11=enabled \
	-Dintrospection=enabled \
	-Ddbus_glib=disabled \
	-Ddocs=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

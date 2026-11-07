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

patch -p1 -i "$PORT_SRC/useradd-default-groups.patch"

meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	--libexecdir=/usr/lib \
	--buildtype=release \
	-Delogind=true \
	-Dadmin_group=wheel \
	-Dwtmpfile=/var/log/wtmp \
	-Dsystemdsystemunitdir=no \
	-Dlibxcrypt=false \
	-Dintrospection=false \
	-Dvapi=false \
	-Ddocbook=false \
	-Dgtk_doc=false \
	-Dtests=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

rm "$PKG/usr/share/dbus-1/system-services/org.freedesktop.Accounts.service"
rmdir "$PKG/usr/share/dbus-1/system-services"

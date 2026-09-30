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

# The bus launcher execs dbus-daemon by the path given here. dbus-broker needs
# systemd, so the default bus is dbus-daemon and use_systemd is off; the bus is
# started by D-Bus activation of org.a11y.Bus, not by a user unit.
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

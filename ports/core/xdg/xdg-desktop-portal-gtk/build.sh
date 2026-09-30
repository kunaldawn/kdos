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

# Only the interfaces xdg-desktop-portal-kdos does not answer are built.
# Settings, AppChooser and Lockdown are the KDOS backend's or nobody's, and
# Wallpaper needs gnome-desktop. Which backend serves an interface is decided
# by kdos-portals.conf, not by this file's UseIn= line.
meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib --libexecdir=/usr/lib \
	--buildtype=release \
	-Ddbus-service-dir=/usr/share/dbus-1/services \
	-Dwallpaper=disabled \
	-Dsettings=disabled \
	-Dappchooser=disabled \
	-Dlockdown=disabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

rm -rf "$PKG/usr/lib/systemd"

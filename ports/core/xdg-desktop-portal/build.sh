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
	-Ddocumentation=disabled \
	-Dman-pages=enabled \
	-Dgeoclue=disabled \
	-Dgudev=enabled \
	-Dsystemd=disabled \
	-Dflatpak-interfaces=disabled \
	-Dtests=disabled \
	-Dinstalled-tests=false \
	-Dsandboxed-image-validation=disabled \
	-Dsandboxed-sound-validation=disabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

# The user units are installed whatever -Dsystemd says: systemd-user-unit-dir
# only moves them, and D-Bus activation is what starts the portals here.
rm -rf "$PKG/usr/lib/systemd"

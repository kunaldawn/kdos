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

# THE VALIDATORS RUN INSIDE bwrap. An icon or a sound a boxed application hands
# a portal is decoded by gdk-pixbuf or GStreamer, and those decoders are where
# the bugs are; the validator re-executes itself under bubblewrap with no
# network, no home and a read-only /usr, so a decoder exploit lands in an empty
# namespace. The path is found at configure and compiled in, which is why
# bubblewrap is in depends.
#
# LOCATION asks the system GeoClue service. The portal is exported only when
# a backend answers Access, which xdg-desktop-portal-kdos does. It asks that
# question only of an application in a sandbox it recognises — Flatpak, Snap
# or Linyaps. A KDOS box is none of them, so the front end treats a boxed
# application as an unsandboxed host program and hands it a position without
# asking.
meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib --libexecdir=/usr/lib \
	-Ddocumentation=disabled \
	-Dman-pages=enabled \
	-Dgeoclue=enabled \
	-Dgudev=enabled \
	-Dsystemd=disabled \
	-Dflatpak-interfaces=disabled \
	-Dtests=disabled \
	-Dinstalled-tests=false \
	-Dsandboxed-image-validation=enabled \
	-Dsandboxed-sound-validation=enabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

# The user units are installed whatever -Dsystemd says: systemd-user-unit-dir
# only moves them, and D-Bus activation is what starts the portals here.
rm -rf "$PKG/usr/lib/systemd"

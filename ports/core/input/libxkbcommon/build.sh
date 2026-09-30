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

# enable-x11 builds libxkbcommon-x11, which reads the keymap from an X server
# over xcb-xkb: Qt's xcb platform plugin needs it for every Qt program under
# Xwayland.
meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	-Denable-wayland=true \
	-Denable-x11=true \
	-Denable-docs=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

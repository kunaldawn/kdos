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

kdos-theme cursors "$PKG/usr/share/icons/KDOS-cursors" phosphor \
	--src "$PORT_SRC/art"
# Distrobox apps share $HOME but not /usr/share/icons — a copy in
# ~/.icons makes the theme visible inside the box too.
mkdir -p "$PKG/etc/skel/.icons"
cp -a "$PKG/usr/share/icons/KDOS-cursors" "$PKG/etc/skel/.icons/"
install -Dm644 "$PORT_SRC/LICENSE.notice" \
	"$PKG/usr/share/licenses/$name/LICENSE.notice"

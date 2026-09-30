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
# The art ships so `kdos theme <accent>` can recolour the cursors on the running
# system — kdos-theme falls back to CURSOR_ART_DEFAULT, which is exactly this
# path. It is 4.4 MB, and without it the cursors were the one part of the palette
# that could not follow an accent switch. kdos-icons ships its art for the same
# reason and in the same shape.
mkdir -p "$PKG/usr/share/kdos/cursors"
cp -a "$PORT_SRC/art" "$PKG/usr/share/kdos/cursors/art"

install -Dm644 "$PORT_SRC/LICENSE.notice" \
	"$PKG/usr/share/licenses/$name/LICENSE.notice"

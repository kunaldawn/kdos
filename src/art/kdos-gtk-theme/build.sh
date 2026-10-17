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

kdos-theme gtk "$PKG/usr/share/themes/KDOS" phosphor \
	--src "$PORT_SRC/theme"

mkdir -p "$PKG/usr/share/kdos/gtk-theme"
cp -a "$PORT_SRC/theme" "$PKG/usr/share/kdos/gtk-theme/theme"

install -Dm644 "$PORT_SRC/LICENSE.notice" \
	"$PKG/usr/share/licenses/kdos-gtk-theme/LICENSE.notice"

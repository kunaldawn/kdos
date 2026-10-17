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
mkdir -p "$PKG/etc/skel/.icons"
cp -a "$PKG/usr/share/icons/KDOS-cursors" "$PKG/etc/skel/.icons/"
install -d "$PKG/usr/share/icons/default"
printf '[Icon Theme]\nName=Default\nComment=The KDOS phosphor cursors\nInherits=KDOS-cursors\n' \
	>"$PKG/usr/share/icons/default/index.theme"
chmod 644 "$PKG/usr/share/icons/default/index.theme"
mkdir -p "$PKG/usr/share/kdos/cursors"
cp -a "$PORT_SRC/art" "$PKG/usr/share/kdos/cursors/art"

install -Dm644 "$PORT_SRC/LICENSE.notice" \
	"$PKG/usr/share/licenses/$name/LICENSE.notice"

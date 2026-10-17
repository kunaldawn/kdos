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

dest="$PKG/usr/share/libretro/assets"
install -d "$dest" "$dest/pkg"
cp -a branding fonts glui ozone rgui sounds xmb "$dest/"
cp -a pkg/fallback-font.ttf pkg/fallback-font.txt pkg/osd-font.ttf "$dest/pkg/"
install -Dm644 COPYING "$PKG/usr/share/licenses/$name/COPYING"

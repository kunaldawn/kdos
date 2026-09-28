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

# The menu drivers' themes, the fonts and sounds they draw with, and the
# branding. Left out: the art sources (src/), the console builds' copies
# (ctr/, switch/, nxrgui/, pkg/wiiu/), the optional wallpaper collection, and
# the Chinese and Korean fallback fonts, which only the extra languages
# RetroArch is built without would reach. The path is the one RetroArch's
# skeleton configuration names.
dest="$PKG/usr/share/libretro/assets"
install -d "$dest" "$dest/pkg"
cp -a branding fonts glui ozone rgui sounds xmb "$dest/"
cp -a pkg/fallback-font.ttf pkg/fallback-font.txt pkg/osd-font.ttf "$dest/pkg/"
install -Dm644 COPYING "$PKG/usr/share/licenses/$name/COPYING"

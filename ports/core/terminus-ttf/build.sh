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

# kpkg copies a .zip into $SRC rather than unpacking it, so the recipe unpacks
# it — with bsdtar, the way docbook-xml already does, rather than adding unzip.
bsdtar -xf "$PORT_SRC/$name-$version.zip"

install -dm755 "$PKG/usr/share/fonts/TTF"
install -m644 "$name-$version"/*.ttf "$PKG/usr/share/fonts/TTF/"

install -dm755 "$PKG/usr/share/licenses/$name"
install -m644 "$name-$version/COPYING" "$PKG/usr/share/licenses/$name/"

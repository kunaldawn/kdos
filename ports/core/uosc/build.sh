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

# ziggy is uosc's helper binary (clipboard, file listing, subtitle search).
# The release zip carries it prebuilt for three systems, so it is built here
# from src/ziggy instead; src/tools is upstream's packaging helper and is not
# built.
tar xf $PORT_SRC/${name}-vendor-${version}.tar.xz
export CGO_ENABLED=0
go build -mod=vendor -trimpath -ldflags "-s -w" -o ziggy-bin ./src/ziggy

# /etc/mpv/scripts is mpv's system script directory, the one mpv-mpris uses,
# so uosc replaces the built-in controller for every user; main.lua sets
# osc=no itself. The binary lives under /usr/lib and main.lua finds it at
# <script dir>/bin/ziggy-linux, which is a link to it.
install -d "$PKG/etc/mpv/scripts" "$PKG/etc/mpv/script-opts"
cp -r src/uosc "$PKG/etc/mpv/scripts/uosc"
# English only: the interface strings are built in, and a missing locale file
# falls back to them.
rm -rf "$PKG/etc/mpv/scripts/uosc/intl"
install -Dm755 ziggy-bin "$PKG/usr/lib/uosc/ziggy"
install -d "$PKG/etc/mpv/scripts/uosc/bin"
ln -s /usr/lib/uosc/ziggy "$PKG/etc/mpv/scripts/uosc/bin/ziggy-linux"
install -Dm644 src/uosc.conf "$PKG/etc/mpv/script-opts/uosc.conf"

# The icon and texture faces are found by family name through fontconfig, so
# they are system fonts rather than files in mpv's per-user fonts directory.
install -Dm644 -t "$PKG/usr/share/fonts/uosc" \
	src/fonts/uosc_icons.otf src/fonts/uosc_textures.ttf
install -Dm644 LICENSE.LGPL "$PKG/usr/share/licenses/uosc/LICENSE.LGPL"

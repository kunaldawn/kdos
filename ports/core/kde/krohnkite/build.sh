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

tar xf "$PORT_SRC/$name-vendor-$version.tar.xz"
export PATH="$SRC/node_modules/.bin:$PATH"

tsc
make pkg PROJECT_VER="$version" PROJECT_REV="$_rev"

install -d "$PKG/usr/share/kwin/scripts"
cp -r pkg "$PKG/usr/share/kwin/scripts/krohnkite"
install -Dm644 res/appicon/krohnkite.svg "$PKG/usr/share/icons/hicolor/scalable/apps/krohnkite.svg"
for f in res/layouts/*.svg; do
	install -Dm644 "$f" "$PKG/usr/share/icons/hicolor/scalable/actions/krohnkite-layout-${f##*/}"
done

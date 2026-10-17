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

pip3 install --ignore-installed --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr ./zenmap
rm -f "$PKG/usr/bin/ndiff"
install -Dm644 docs/zenmap.1 -t "$PKG/usr/share/man/man1"
install -Dm644 zenmap/install_scripts/unix/zenmap.desktop "$PKG/usr/share/applications/zenmap.desktop"
install -Dm644 zenmap/zenmapCore/data/pixmaps/zenmap.png "$PKG/usr/share/icons/hicolor/256x256/apps/zenmap.png"

test -x "$PKG/usr/bin/zenmap"
test -z "$(find "$PKG" -name 'ndiff*')"

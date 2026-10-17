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

osinfo-db-validate --dir "$SRC"
install -d "$PKG/usr/share/osinfo"
cp -R . "$PKG/usr/share/osinfo/"
find "$PKG/usr/share/osinfo" -type d -exec chmod 755 {} +
find "$PKG/usr/share/osinfo" -type f -exec chmod 644 {} +

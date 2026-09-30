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

# The game databases the scanner names content by, and the cursors that query
# them. Left out: the cheat collection (234 MB, the largest part of the tree)
# and the dat and metadat sources the databases are compiled from. The path is
# the one RetroArch's skeleton configuration names.
dest="$PKG/usr/share/libretro/database"
install -d "$dest"
cp -a rdb cursors "$dest/"
install -Dm644 LICENSE "$PKG/usr/share/licenses/$name/LICENSE"

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

# The crosextrafonts release is the Carlito LibreOffice pins: the later
# googlefonts builds are drawn from sources only fontmake compiles.
install -dm755 $PKG/usr/share/fonts/carlito
install -m644 Carlito-*.ttf $PKG/usr/share/fonts/carlito/
install -Dm644 LICENSE $PKG/usr/share/licenses/$name/LICENSE

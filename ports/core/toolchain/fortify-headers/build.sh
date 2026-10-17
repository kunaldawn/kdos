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

patch -p1 -i "$PORT_SRC/late-feature-macro.patch"
patch -p1 -i "$PORT_SRC/lto-no-always-inline.patch"
install -d "$PKG/usr/include/fortify/sys"
install -m644 include/*.h "$PKG/usr/include/fortify/"
install -m644 include/sys/*.h "$PKG/usr/include/fortify/sys/"

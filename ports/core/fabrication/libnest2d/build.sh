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


install -d "$PKG/usr/include"
cp -r include/libnest2d "$PKG/usr/include/"
find "$PKG/usr/include/libnest2d" -type f -exec chmod 644 {} +

test -f "$PKG/usr/include/libnest2d/libnest2d.hpp"
test -f "$PKG/usr/include/libnest2d/backends/clipper/geometries.hpp"

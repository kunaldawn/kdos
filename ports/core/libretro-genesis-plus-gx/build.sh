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

# The licence forbids selling the core or using it in a commercial product,
# so it travels beside the core.
make -f Makefile.libretro platform=unix
install -Dm755 genesis_plus_gx_libretro.so "$PKG/usr/lib/libretro/genesis_plus_gx_libretro.so"
install -Dm644 LICENSE.txt "$PKG/usr/share/licenses/$name/LICENSE.txt"

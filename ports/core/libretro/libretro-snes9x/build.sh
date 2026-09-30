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

# The core only, from upstream's release tree. Its licence forbids commercial
# distribution, so it travels beside the core.
make -C libretro platform=unix
install -Dm755 libretro/snes9x_libretro.so "$PKG/usr/lib/libretro/snes9x_libretro.so"
install -Dm644 LICENSE "$PKG/usr/share/licenses/$name/LICENSE"

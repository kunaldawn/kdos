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

# OpenGL renderer and the x86_64 JIT, which the unix platform turns on for
# this architecture. The core carries free replacements for the DS BIOS and
# firmware, so no dumped firmware is needed to boot a game.
make platform=unix
install -Dm755 melonds_libretro.so "$PKG/usr/lib/libretro/melonds_libretro.so"
install -Dm644 LICENSE "$PKG/usr/share/licenses/$name/LICENSE"

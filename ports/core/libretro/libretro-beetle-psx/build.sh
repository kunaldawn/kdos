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

# Two cores from one tree, as the core info files name them:
# mednafen_psx is the software renderer alone, mednafen_psx_hw adds the
# OpenGL and Vulkan renderers. Both share one object directory, so the tree is
# cleaned between them. libstdc++ is linked shared: a static copy inside a
# core would be a second C++ runtime in RetroArch's process.
make platform=unix LINK_STATIC_LIBCPLUSPLUS=0 HAVE_HW=0
install -Dm755 mednafen_psx_libretro.so "$PKG/usr/lib/libretro/mednafen_psx_libretro.so"
make platform=unix LINK_STATIC_LIBCPLUSPLUS=0 HAVE_HW=0 clean
make platform=unix LINK_STATIC_LIBCPLUSPLUS=0 HAVE_HW=1
install -Dm755 mednafen_psx_hw_libretro.so "$PKG/usr/lib/libretro/mednafen_psx_hw_libretro.so"
install -Dm644 COPYING "$PKG/usr/share/licenses/$name/COPYING"

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

make platform=unix LINK_STATIC_LIBCPLUSPLUS=0 HAVE_HW=0
install -Dm755 mednafen_psx_libretro.so "$PKG/usr/lib/libretro/mednafen_psx_libretro.so"
make platform=unix LINK_STATIC_LIBCPLUSPLUS=0 HAVE_HW=0 clean
make platform=unix LINK_STATIC_LIBCPLUSPLUS=0 HAVE_HW=1
install -Dm755 mednafen_psx_hw_libretro.so "$PKG/usr/lib/libretro/mednafen_psx_hw_libretro.so"
install -Dm644 COPYING "$PKG/usr/share/licenses/$name/COPYING"

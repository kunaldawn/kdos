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

patch -p1 -i "$PORT_SRC/gcc14.patch"
patch -p1 -i "$PORT_SRC/gcc15.patch"

# Desktop GL through libglvnd, the x86_64 dynamic recompiler (assembled with
# nasm), and the system zlib, libpng, minizip and xxHash rather than the
# copies in the tree, so a fix to any of them reaches the core.
make platform=unix WITH_DYNAREC=x86_64 \
	SYSTEM_ZLIB=1 SYSTEM_LIBPNG=1 SYSTEM_MINIZIP=1 SYSTEM_XXHASH=1
install -Dm755 mupen64plus_next_libretro.so "$PKG/usr/lib/libretro/mupen64plus_next_libretro.so"
install -Dm644 LICENSE "$PKG/usr/share/licenses/$name/LICENSE"

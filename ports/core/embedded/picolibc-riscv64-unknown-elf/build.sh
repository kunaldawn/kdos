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

unset CFLAGS CXXFLAGS LDFLAGS
meson setup build \
	--cross-file scripts/cross-riscv64-unknown-elf.txt \
	--prefix=/usr/riscv64-unknown-elf \
	-Dsysroot-install=true \
	-Dtests=false \
	--buildtype=release
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

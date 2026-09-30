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

# THE C LIBRARY IS BUILT WITH THE CROSS COMPILER, WHICH IS WHY IT IS A SEPARATE
# PORT. gcc-riscv64-unknown-elf is built --without-headers --with-newlib
# precisely so libgcc can exist before this does; this then compiles against
# that gcc and lands in its sysroot. One recipe cannot do both.
#
# UPSTREAM SHIPS THE CROSS FILE. scripts/cross-riscv64-unknown-elf.txt names
# the compiler, the archiver and the host machine, so there is nothing here to
# invent — and inventing one is how a cross build silently picks up the host's
# own gcc.
meson setup build \
	--cross-file scripts/cross-riscv64-unknown-elf.txt \
	--prefix=/usr/riscv64-unknown-elf \
	-Dsysroot-install=true \
	-Dtests=false \
	--buildtype=release
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

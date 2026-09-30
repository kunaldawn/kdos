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

meson setup build \
	--prefix=/usr \
	--libdir=lib \
	--buildtype=release \
	-D mmx=enabled \
	-D sse2=enabled \
	-D ssse3=enabled \
	-D loongson-mmi=disabled \
	-D vmx=disabled \
	-D arm-simd=disabled \
	-D neon=disabled \
	-D a64-neon=disabled \
	-D mips-dspr2=disabled \
	-D rvv=disabled \
	-D openmp=disabled \
	-D libpng=disabled \
	-D tests=disabled \
	-D demos=disabled \
	-D gtk=disabled

ninja -C build
DESTDIR=$PKG ninja -C build install

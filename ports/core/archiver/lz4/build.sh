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

# LTO WITH A FIXED SEED. GCC names the .gnu.lto_* sections of every object
# with a random number unless -frandom-seed gives one, and liblz4.a keeps the
# fat objects, so without it the archive differs on every build.
lto="-flto=auto -ffat-lto-objects -frandom-seed=lz4"
export CFLAGS="${CFLAGS/-O2/-O3} $lto" CXXFLAGS="${CXXFLAGS/-O2/-O3} $lto"
export LDFLAGS="$LDFLAGS -flto=auto"
make PREFIX=/usr
make DESTDIR=$PKG PREFIX=/usr install

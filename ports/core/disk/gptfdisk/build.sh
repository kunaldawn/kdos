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

mkdir -p kdos-inc/ncursesw
ln -sf /usr/include/ncurses.h kdos-inc/ncursesw/ncurses.h
export CXXFLAGS="$CXXFLAGS -D_LARGEFILE64_SOURCE -I$PWD/kdos-inc"
make gdisk cgdisk sgdisk fixparts
install -d $PKG/usr/bin $PKG/usr/share/man/man8
install -t $PKG/usr/bin gdisk cgdisk sgdisk fixparts
install -m644 -t $PKG/usr/share/man/man8 gdisk.8 cgdisk.8 sgdisk.8 fixparts.8

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

# cgdisk includes <ncursesw/ncurses.h>, and this tree's ncurses installs its
# wide headers straight into /usr/include with no ncursesw/ directory, so the
# build gets one of its own pointing at the real header.
mkdir -p kdos-inc/ncursesw
ln -sf /usr/include/ncurses.h kdos-inc/ncursesw/ncurses.h
export CXXFLAGS="$CXXFLAGS -D_LARGEFILE64_SOURCE -I$PWD/kdos-inc"
make gdisk cgdisk sgdisk fixparts
install -d $PKG/usr/bin $PKG/usr/share/man/man8
install -t $PKG/usr/bin gdisk cgdisk sgdisk fixparts
install -m644 -t $PKG/usr/share/man/man8 gdisk.8 cgdisk.8 sgdisk.8 fixparts.8

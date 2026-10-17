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

mkdir -p compat/ncursesw
ln -sf /usr/include/ncurses.h compat/ncursesw/ncurses.h
_cflags="$CFLAGS -I. -I$PWD/compat -Wall -D_GNU_SOURCE -D_XOPEN_SOURCE_EXTENDED -fPIC"

make FOUND_SWIG=0 FOUND_SPL=0 CFLAGS="$_cflags" LDLIBS="-Wl,-z,now,-z,pack-relative-relocs -lncursesw"
make FOUND_SWIG=0 FOUND_SPL=0 CFLAGS="$_cflags" LDLIBS="-Wl,-z,now,-z,pack-relative-relocs -lncursesw" \
	prefix=/usr libdir=lib DESTDIR=$PKG install

ln -sf libstfl.so.$version $PKG/usr/lib/libstfl.so.0

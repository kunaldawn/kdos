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

# The library, the `aspell` command, and prezip-bin, which the dictionary
# ports need to build. Curses is the wide ncurses, for `aspell check`'s
# interactive screen. NLS is off: it would put a libintl link under a library
# that weechat, mc, recoll and enchant all load, for message catalogues nothing
# on this image selects.
./configure \
	--prefix=/usr \
	--sysconfdir=/etc \
	--libdir=/usr/lib \
	--disable-static \
	--disable-nls \
	--enable-curses=ncursesw
make
make DESTDIR=$PKG install

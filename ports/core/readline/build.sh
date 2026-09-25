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

# Upstream's official patch series, in order. Each is a diff from the top of
# the release tree, so -p0 from $SRC.
for p in "$_pfx"-[0-9][0-9][0-9]; do
	patch -p0 -i "$p"
done

# The termcap probe takes the first library carrying tgetent, which here is the
# libcurses compatibility link; the cache variable names ncursesw instead, so
# readline.pc requires ncursesw and --with-shared-termcap-library links the
# shared library against it. Without that link every consumer that does not
# name ncurses itself fails on an undefined tgetent.
./configure --host=$TARGET --prefix=/usr \
	--with-curses --with-shared-termcap-library \
	--disable-static --disable-install-examples \
	bash_cv_termcap_lib=libncursesw
make
make DESTDIR=$PKG install

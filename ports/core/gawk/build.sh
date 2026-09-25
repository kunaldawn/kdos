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

# The same recipe builds in 02_phase2. Every optional library is either
# declared and pinned on, or pinned off, so no feature follows build order:
# mpfr and gmp are declared, so -M is on; readline is declared, so the
# debugger (-D) has line editing and history, and the bootstrap builds ncurses
# and readline ahead of gawk; gettext is not declared and the image ships no
# message catalogues, so NLS is off and the binary never links libintl_gettext.
#
# --sysconfdir=/etc puts gawk.sh, the gawkpath_* and gawklibpath_* helpers,
# in the /etc/profile.d a login shell reads; the default is /usr/etc, which
# nothing reads. Its csh twin goes: no csh is installed.
./configure --prefix=/usr \
	--sysconfdir=/etc \
	--with-mpfr \
	--with-readline \
	--disable-nls
make
make DESTDIR=$PKG install
rm -f "$PKG/etc/profile.d/gawk.csh"

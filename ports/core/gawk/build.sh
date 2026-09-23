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

# The same recipe builds in 02_phase2, where neither readline nor gettext is
# reachable: declaring them would pull both into the bootstrap, and leaving
# them to probe would make the debugger's line editing, and whether the binary
# links libintl_gettext at all, follow build order. The image ships no message
# catalogues. mpfr and gmp are declared, so -M is pinned on.
./configure --prefix=/usr \
	--with-mpfr \
	--without-readline \
	--disable-nls
make
make DESTDIR=$PKG install

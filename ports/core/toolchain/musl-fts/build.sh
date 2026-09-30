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

# bootstrap.sh is autoreconf by hand, and configure.ac's PKG_INSTALLDIR comes
# from pkgconf's pkg.m4; it sets pkgconfigdir to $(libdir)/pkgconfig. All four
# tools are in `depends`, so the macro is always there to expand.
./bootstrap.sh
./configure \
	--prefix=/usr \
	--sysconfdir=/etc \
	--libdir=/usr/lib

make
make DESTDIR=$PKG install
install -Dm644 fts.3 -t $PKG/usr/share/man/man3

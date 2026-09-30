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

# NOT ./autogen.sh: it invokes aclocal-1.16 BY VERSIONED NAME and exits when
# that exact automake is not the one installed. autoreconf finds whichever is.
autoreconf -fi
# The transfer menu runs sz and rz from lrzsz; kermit and lockdev are not
# ports, and each is found by searching when left unpinned.
./configure --prefix=/usr --sysconfdir=/etc --enable-lock-dir=/run/lock \
	--enable-nls \
	--disable-lockdev \
	--disable-kermit
make
make DESTDIR=$PKG install

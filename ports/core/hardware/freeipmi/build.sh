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

export LDFLAGS="$LDFLAGS -Wl,-z,stack-size=0x200000"
./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--sysconfdir=/etc \
	--localstatedir=/var \
	--mandir=/usr/share/man \
	--disable-static \
	--disable-init-scripts \
	--without-systemdsystemunitdir
grep -q '^#define HAVE_ARGP_H 1' config/config.h
grep -q '^#define WITH_ENCRYPTION 1' config/config.h
make
make DESTDIR=$PKG install

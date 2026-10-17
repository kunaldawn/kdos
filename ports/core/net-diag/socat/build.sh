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

patch -p1 -i "$PORT_SRC/openssl4.patch"

./configure --prefix=/usr --mandir=/usr/share/man \
	--enable-readline \
	--enable-openssl \
	--disable-libwrap
grep -q '^#define WITH_READLINE 1' config.h
grep -q '^#define WITH_OPENSSL 1' config.h
make
make DESTDIR=$PKG install

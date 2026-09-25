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
# --enable-readline and --enable-openssl only restate the defaults, and a
# library either one cannot find is a warning and a socat without READLINE or
# OPENSSL addresses. config.h is where the outcome is written down.
grep -q '^#define WITH_READLINE 1' config.h
grep -q '^#define WITH_OPENSSL 1' config.h
make
make DESTDIR=$PKG install

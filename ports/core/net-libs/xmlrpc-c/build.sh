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

export CFLAGS="$CFLAGS -g0" CXXFLAGS="$CXXFLAGS -g0"
./configure --prefix=/usr --libdir=/usr/lib --mandir=/usr/share/man \
	--enable-curl-client \
	--enable-abyss-server \
	--enable-abyss-threads \
	--enable-abyss-openssl \
	--enable-cplusplus \
	--disable-libxml2-backend \
	--disable-cgi-server \
	--disable-libwww-client \
	--disable-wininet-client
make -j1
make -j1 -C tools
make -j1 DESTDIR=$PKG install
make -j1 -C tools DESTDIR=$PKG install

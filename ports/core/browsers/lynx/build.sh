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

./configure \
	--prefix=/usr \
	--sysconfdir=/etc/lynx \
	--datadir=/usr/share/lynx \
	--with-ssl \
	--with-zlib \
	--with-bzlib \
	--with-brotli \
	--with-zstd \
	--enable-idna \
	--enable-ipv6 \
	--enable-nls \
	--enable-nsl-fork \
	--enable-nested-tables \
	--enable-default-colors \
	--enable-widec \
	--with-screen=ncursesw
make
make DESTDIR=$PKG install

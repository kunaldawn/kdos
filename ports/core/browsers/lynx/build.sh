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

# bzip2, brotli, zstd and IDNA are probed whether named or not, and a missing
# library costs only a configure warning, not a failure: the depends line is
# what keeps them in the build. IDNA takes idn2 before the older idn.
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

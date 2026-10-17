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

patch -p1 -i "$PORT_SRC/musl-unistd.patch"
./configure \
	--prefix=/usr \
	--sysconfdir=/etc \
	--libdir=/usr/lib \
	--enable-fuse \
	--enable-python \
	--with-gnutls \
	--with-libxml2 \
	--enable-ublk \
	--disable-ocaml \
	--disable-golang \
	--disable-rust \
	--with-bash-completions \
	--disable-static
make
make DESTDIR=$PKG install
test -x "$PKG/usr/bin/nbdublk"

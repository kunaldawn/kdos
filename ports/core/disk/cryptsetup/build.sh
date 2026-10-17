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
	--with-crypto_backend=openssl \
	--enable-udev \
	--enable-blkid \
	--disable-pwquality \
	--disable-passwdqc \
	--without-tmpfilesdir \
	--disable-asciidoc \
	--disable-nls \
	--disable-rpath \
	--disable-ssh-token
make
make DESTDIR=$PKG install

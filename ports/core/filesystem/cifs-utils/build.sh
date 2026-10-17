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

export CFLAGS="$CFLAGS -std=gnu17"

./configure \
	--prefix=/usr \
	--sbindir=/usr/sbin \
	--enable-cifsupcall \
	--enable-cifscreds \
	--enable-smbinfo \
	--enable-pythontools \
	--with-libcap-ng=yes \
	--disable-cifsidmap \
	--disable-cifsacl \
	--disable-pam \
	--disable-systemd \
	--enable-man
make
make install DESTDIR=$PKG ROOTSBINDIR=/usr/sbin

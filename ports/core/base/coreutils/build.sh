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

export FORCE_UNSAFE_CONFIGURE=1
fu_cv_sys_stat_statvfs=n ./configure --prefix=/usr --disable-nls --without-openssl \
	--without-selinux --with-libgmp --enable-acl --enable-xattr --enable-libcap \
	--enable-install-program=arch,hostname \
	CPPFLAGS=-I/usr/include/utmps LIBS="-lutmps -lskarnet"
make
make DESTDIR=$PKG install

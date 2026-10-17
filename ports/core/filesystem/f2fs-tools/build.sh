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

autoreconf -f -i -s

export CPPFLAGS="${CPPFLAGS:-} -DHAVE_LSEEK64=1"

export CFLAGS="${CFLAGS:-} -std=gnu17"

./configure \
	ac_cv_lib_uuid_uuid_clear=yes \
	--prefix=/usr \
	--libdir=/usr/lib \
	--sbindir=/usr/sbin \
	--disable-static \
	--with-blkid \
	--with-lz4 \
	--with-lzo2 \
	--without-selinux
make
make DESTDIR=$PKG install

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

# A git snapshot carries no configure; autogen.sh is a wrapper around this.
autoreconf -f -i -s

# --without-selinux because there is no libselinux on the host. The other three
# are libraries this tree already ships, and a missing one here is answered by
# silently dropping the feature rather than by failing: without blkid,
# mkfs.f2fs cannot see that it is about to overwrite a filesystem, and without
# lz4/lzo2 the compressed-file features are absent from a binary whose recipe
# claims them.
./configure \
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

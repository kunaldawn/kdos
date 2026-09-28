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

# configure refuses to run without an ldconfig, which musl does not have; the
# value is only probed, never run by the install. /sbin is a link to /usr/sbin
# and /usr/sbin is its own directory, so the tools go to /usr/sbin, where the
# mount helper looks for them.
LDCONFIG=/bin/true ./configure --prefix=/usr --sysconfdir=/etc \
	--libdir=/usr/lib --disable-static \
	--enable-usrmerge=sbin \
	--with-libmount \
	--with-blkid \
	--without-selinux
make
make DESTDIR=$PKG install

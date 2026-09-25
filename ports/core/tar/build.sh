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

# ACLs ARE OFF because tar is built in phase 2, where acl and attr do not
# exist; declaring acl would pull both into the bootstrap. Left to detection,
# the result would depend on whether a later tree already had libacl.
FORCE_UNSAFE_CONFIGURE=1 ./configure \
	--prefix=/usr \
	--disable-nls \
	--disable-acl \
	--without-posix-acls \
	--without-selinux \
	--with-xattrs
make
make DESTDIR=$PKG install

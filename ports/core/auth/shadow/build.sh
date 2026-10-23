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

patch -p1 -i "$PORT_SRC/no-mail-spool.patch"

./configure \
	--prefix=/usr \
	--bindir=/usr/bin \
	--sbindir=/usr/sbin \
	--libdir=/usr/lib \
	--sysconfdir=/etc \
	--disable-logind \
	--disable-static \
	--disable-rpath \
	--disable-nls \
	--enable-lastlog \
	--with-bcrypt \
	--without-yescrypt \
	--without-libbsd \
	--without-libpam \
	--with-acl \
	--with-btrfs \
	--without-audit \
	--without-selinux \
	--without-tcb \
	--with-group-name-max-length=32 \
	shadow_cv_maildir=none \
	shadow_cv_mailfile=none \
	CPPFLAGS=-I/usr/include/utmps LIBS="-lutmps -lskarnet"

make
make DESTDIR=$PKG install

chmod 4755 "$PKG/usr/bin/newuidmap" "$PKG/usr/bin/newgidmap"

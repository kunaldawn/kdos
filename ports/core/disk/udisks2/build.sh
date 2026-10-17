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

./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--libexecdir=/usr/lib --localstatedir=/var --disable-static \
	--enable-introspection=yes \
	--enable-daemon \
	--enable-man \
	--disable-gtk-doc \
	--disable-nls \
	--enable-acl \
	--enable-lvm2 \
	--enable-btrfs \
	--disable-iscsi \
	--enable-lsm \
	--enable-smart \
	--disable-fhs-media \
	--with-udevdir=/usr/lib/udev \
	--with-systemdsystemunitdir=no \
	--with-tmpfilesdir=no
make
make DESTDIR=$PKG install

rm -rf "$PKG/usr/lib/modules-load.d" "$PKG/usr/lib/tmpfiles.d"

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

export DEBUG=-DNDEBUG OPTIMIZER=-O2
./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--enable-lib64=no \
	--localstatedir=/var \
	--sbindir=/usr/sbin \
	--enable-gettext=yes \
	--enable-editline=yes \
	--enable-termcap=no \
	--enable-scrub=yes \
	--enable-libicu=yes \
	--enable-healer=yes \
	--enable-lto=no \
	--with-systemd-unit-dir=no \
	--with-udev-rule-dir=no \
	--with-crond-dir=no \
	ac_cv_search_dm_task_create=no

make

make -j1 DESTDIR=$PKG install install-dev

rm -f $PKG/usr/sbin/xfs_scrub_all $PKG/usr/share/man/man8/xfs_scrub_all.8

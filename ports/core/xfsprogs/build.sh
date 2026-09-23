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

export DEBUG=-DNDEBUG
# The unit, udev-rule and crontab directories stay off: every file they
# install exists only to start the xfs_scrub systemd services, and the udev
# rule would otherwise land wherever eudev's udev.pc points.
# ac_cv_search_dm_task_create=no keeps xfs_io's dm-log-writes replay, a test
# aid, from linking libdevmapper whenever lvm2 happens to be installed.
./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
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

# xfs_scrub_all drives the per-mount scrub through systemd over D-Bus and
# imports python-dbus at start; xfs_scrub itself runs directly.
rm -f $PKG/usr/sbin/xfs_scrub_all

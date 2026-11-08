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

./configure \
	--prefix=/usr \
	--sysconfdir=/etc \
	--localstatedir=/var \
	--runstatedir=/run \
	--disable-anacron \
	--disable-syscrontab \
	--with-inotify \
	--without-pam \
	--without-selinux \
	--without-audit \
	--with-editor=/usr/bin/nano
make
make DESTDIR=$PKG install

chmod 4755 "$PKG/usr/bin/crontab"
install -d -m 755 "$PKG/etc/cron.d"
install -m 644 /dev/null "$PKG/etc/cron.deny"

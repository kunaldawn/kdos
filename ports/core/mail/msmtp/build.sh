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
	--with-tls=gnutls \
	--with-libidn \
	--without-libgsasl \
	--without-libsecret \
	--without-msmtpd \
	--disable-nls
make
make DESTDIR=$PKG install

install -d "$PKG/usr/sbin"
ln -sf ../bin/msmtp "$PKG/usr/sbin/sendmail"
ln -sf msmtp "$PKG/usr/bin/sendmail"

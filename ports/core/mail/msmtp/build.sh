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

# THE PROGRAM ANYTHING THAT SENDS MAIL ALREADY KNOWS HOW TO FIND. aerc, cron,
# mailx and a shell script all hand a message to `sendmail` on stdin, so the
# two links at the bottom are what make this the machine's transport rather
# than one client's setting.
#
# --with-tls=gnutls: this tree already builds gnutls for cups and chrony, and a
# second TLS stack would be a second trust store to keep current.
# --with-libidn RESOLVES AGAINST libidn2 despite the flag's name — the help
# string says "needs GNU Libidn" and the generated configure asks pkg-config
# for `libidn2`, which is the port named in depends.
# --disable-nls: gettext here is built --disable-nls, so a port that links
# libintl finds nothing to link and fails the way newsboat's comment records.
# --without-msmtpd: msmtpd is an SMTP server on localhost. Nothing on this
# image submits over a socket, and a listener nobody uses is still a listener.
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

# TWO HARDCODED PATHS, ONE TRANSPORT. Programs written against Debian call
# /usr/sbin/sendmail and everything else calls /usr/bin/sendmail; a machine
# that answers only one of them has a mail path that works for half the
# software on it. Relative targets, so the links resolve inside $PKG, inside
# the chroot and on the installed system alike.
install -d "$PKG/usr/sbin"
ln -sf ../bin/msmtp "$PKG/usr/sbin/sendmail"
ln -sf msmtp "$PKG/usr/bin/sendmail"

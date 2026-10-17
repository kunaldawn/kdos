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

export CFLAGS="$CFLAGS -std=gnu17 -include time.h"

./configure \
	--prefix=/usr \
	--sysconfdir=/etc \
	--with-plugindir=/usr/lib/sasl2 \
	--with-openssl \
	--with-dblib=none \
	--without-pam \
	--without-saslauthd \
	--without-authdaemond \
	--disable-sample \
	--disable-gssapi \
	--disable-krb4 \
	--disable-otp \
	--disable-srp \
	--disable-ntlm \
	--disable-digest \
	--disable-cram \
	--disable-sql \
	--disable-ldapdb \
	--enable-plain \
	--enable-login \
	--enable-scram \
	--enable-shared \
	--disable-static

make -j1
make DESTDIR=$PKG install
install -Dm644 utils/pluginviewer.8 -t "$PKG/usr/share/man/man8"

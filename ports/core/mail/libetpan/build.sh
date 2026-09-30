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

# The forge archive carries no configure. automake runs with --foreign because
# the tree has README.md and no README, which the default GNU strictness
# refuses.
AUTOMAKE="automake --foreign" autoreconf -fi

# TLS is OpenSSL; GnuTLS is the other choice and each is an auto probe, so
# naming one and refusing the other keeps the backend fixed. The on-disk
# message cache would be Berkeley DB or LMDB, and nothing here reads it.
#
# curl and expat are for the RSS "newsfeed" driver only, and both probes call
# the function with no prototype, which this compiler rejects, so they are
# off by name rather than silently.
#
# --enable-iconv, --enable-debug and --disable-debug are NOT passed. iconv's
# option turns iconv OFF whenever it is given at all, and debug's adds -DDEBUG
# whenever it is given at all, either spelling.
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--disable-static \
	--with-openssl \
	--without-gnutls \
	--with-sasl \
	--without-curl \
	--without-expat \
	--with-zlib \
	--enable-threads \
	--enable-ipv6 \
	--disable-db \
	--disable-lmdb \
	--disable-lockfile
make
make DESTDIR=$PKG install

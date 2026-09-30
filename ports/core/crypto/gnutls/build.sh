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
	--with-default-trust-store-pkcs11="pkcs11:" \
	--with-zlib \
	--with-zstd \
	--with-brotli \
	--with-tpm2 \
	--without-tpm \
	--without-leancrypto \
	--disable-libdane \
	--enable-ktls
sed -i -e 's/ -shared / -Wl,-O1,--as-needed\0/g' libtool
make
make DESTDIR=$PKG install

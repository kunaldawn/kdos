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

gnutls_cv_dlopen_soname_works=yes \
./configure \
	--prefix=/usr \
	--sysconfdir=/etc \
	--with-default-trust-store-pkcs11="pkcs11:" \
	--with-zlib=link \
	--with-zstd=link \
	--with-brotli=link \
	--with-tpm2=dlopen \
	--without-tpm \
	--without-leancrypto \
	--disable-libdane \
	--enable-ktls
for _l in ESYS MU TCTILDR; do
	grep -q "^#define TSS2_${_l}_LIBRARY_SONAME \"libtss2-" config.h
done
sed -i -e 's/ -shared / -Wl,-O1,--as-needed\0/g' libtool
make
make DESTDIR=$PKG install
if readelf -d "$PKG/usr/lib/libgnutls.so.30" | grep -q 'NEEDED.*libtss2'; then
	exit 1
fi

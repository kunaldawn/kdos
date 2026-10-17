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

autoreconf -f -i

export CXXFLAGS="$CXXFLAGS -fno-devirtualize-speculatively"

./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--disable-static \
	--disable-python \
	--disable-s3 \
	--without-curl \
	--with-expat \
	--enable-fuse

for have in HAVE_LIBZ HAVE_LIBCRYPTO HAVE_LIBEXPAT HAVE_LIBREADLINE USE_FUSE; do
	if ! grep -q "^#define $have 1" affconfig.h; then
		echo "afflib: configure did not find $have" >&2
		exit 1
	fi
done

make CFLAGS="$CFLAGS" CXXFLAGS="$CXXFLAGS"
make DESTDIR=$PKG install

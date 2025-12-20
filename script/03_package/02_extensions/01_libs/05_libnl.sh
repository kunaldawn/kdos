#!/bin/bash
set -e
source script/env.sh

if [ -f "$SYSROOT/usr/lib/libnl-3.a" ]; then
    exit 0
fi

echo ">>> Building Libnl $LIBNL_VER..."

mkdir -p $BUILD_DIR/tmp
cd $BUILD_DIR/tmp

export CROSS_COMPILE=$TARGET-
export CC=$TARGET-gcc
export CXX=$TARGET-g++
export AR=$TARGET-ar
export RANLIB=$TARGET-ranlib

tar -xf $SRC_DIR/libnl-$LIBNL_VER.tar.gz
cd libnl-$LIBNL_VER
# Libnl 3.x uses cmake or autotools. Check if configure exists, usually does for release tarballs.
# Musl note: might need -D_GNU_SOURCE to find some symbols
./configure --host=$TARGET --prefix=/usr --sysconfdir=/etc --disable-shared \
    CFLAGS="-O2 -pipe --sysroot=$SYSROOT -D_GNU_SOURCE -fPIC"
make
make install DESTDIR=$SYSROOT
cd ..
rm -rf libnl-$LIBNL_VER

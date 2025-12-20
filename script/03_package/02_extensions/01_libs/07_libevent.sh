#!/bin/bash
set -e
source script/env.sh

if [ -f "$SYSROOT/usr/lib/libevent.a" ]; then
    exit 0
fi

echo ">>> Building Libevent $LIBEVENT_VER..."

mkdir -p $BUILD_DIR/tmp
cd $BUILD_DIR/tmp

export CROSS_COMPILE=$TARGET-
export CC=$TARGET-gcc
export CXX=$TARGET-g++
export AR=$TARGET-ar
export RANLIB=$TARGET-ranlib

tar -xf $SRC_DIR/libevent-$LIBEVENT_VER.tar.gz
cd libevent-$LIBEVENT_VER
./configure --host=$TARGET --prefix=/usr --disable-shared --disable-openssl
make
make install DESTDIR=$SYSROOT
cd ..
rm -rf libevent-$LIBEVENT_VER

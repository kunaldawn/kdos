#!/bin/bash
set -e
source script/env.sh

if [ -f "$SYSROOT/usr/lib/libz.a" ]; then
    exit 0
fi

echo ">>> Building Zlib $ZLIB_VER..."

mkdir -p $BUILD_DIR/tmp
cd $BUILD_DIR/tmp

export CROSS_COMPILE=$TARGET-
export CC=$TARGET-gcc
export CXX=$TARGET-g++
export AR=$TARGET-ar
export RANLIB=$TARGET-ranlib

tar -xf $SRC_DIR/zlib-$ZLIB_VER.tar.gz
cd zlib-$ZLIB_VER
export CFLAGS="-fPIC"
./configure --prefix=/usr --static
make
make install DESTDIR=$SYSROOT
cd ..
rm -rf zlib-$ZLIB_VER

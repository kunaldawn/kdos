#!/bin/bash
set -e
source script/env.sh

if [ -f "$SYSROOT/usr/lib/libffi.a" ]; then
    exit 0
fi

echo ">>> Building Libffi $LIBFFI_VER..."

mkdir -p $BUILD_DIR/tmp
cd $BUILD_DIR/tmp

export CROSS_COMPILE=$TARGET-
export CC=$TARGET-gcc
export CXX=$TARGET-g++
export AR=$TARGET-ar
export RANLIB=$TARGET-ranlib

tar -xf $SRC_DIR/libffi-$LIBFFI_VER.tar.gz
cd libffi-$LIBFFI_VER
./configure --host=$TARGET --prefix=/usr --disable-shared CFLAGS="-fPIC"
make
make install DESTDIR=$SYSROOT
cd ..
rm -rf libffi-$LIBFFI_VER

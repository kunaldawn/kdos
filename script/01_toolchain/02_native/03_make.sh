#!/bin/bash
set -e
source script/env.sh

if [ -f "$SYSROOT/usr/bin/make" ]; then
    exit 0
fi

echo ">>> Building Native Make $MAKE_VER..."
mkdir -p $BUILD_DIR/tmp
cd $BUILD_DIR/tmp

export CROSS_COMPILE=$TARGET-
export CC=$TARGET-gcc
export CXX=$TARGET-g++

tar -xf $SRC_DIR/make-$MAKE_VER.tar.gz
cd make-$MAKE_VER
./configure --host=$TARGET --prefix=/usr --disable-nls \
    CFLAGS="-std=gnu99 -O2 -pipe --sysroot=$SYSROOT"
make
make install DESTDIR=$SYSROOT
cd ..
rm -rf make-$MAKE_VER

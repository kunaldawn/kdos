#!/bin/bash
set -e
source script/env.sh

if [ -f "$SYSROOT/sbin/dropbear" ]; then
    exit 0
fi

echo ">>> Building Dropbear $DROPBEAR_VER..."

mkdir -p $BUILD_DIR/tmp
cd $BUILD_DIR/tmp

export CROSS_COMPILE=$TARGET-
export CC=$TARGET-gcc
export CXX=$TARGET-g++
export AR=$TARGET-ar
export RANLIB=$TARGET-ranlib

tar -xf $SRC_DIR/dropbear-$DROPBEAR_VER.tar.bz2
cd dropbear-$DROPBEAR_VER
./configure --host=$TARGET --prefix=/ --enable-zlib --enable-static
make
make install DESTDIR=$SYSROOT
cd ..
rm -rf dropbear-$DROPBEAR_VER

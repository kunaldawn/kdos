#!/bin/bash
set -e
source script/env.sh

if [ -f "$SYSROOT/usr/bin/htop" ]; then
    exit 0
fi

echo ">>> Building Htop $HTOP_VER..."

mkdir -p $BUILD_DIR/tmp
cd $BUILD_DIR/tmp

export CROSS_COMPILE=$TARGET-
export CC=$TARGET-gcc
export CXX=$TARGET-g++
export AR=$TARGET-ar
export RANLIB=$TARGET-ranlib

tar -xf $SRC_DIR/htop-$HTOP_VER.tar.xz
cd htop-$HTOP_VER
./configure --host=$TARGET --prefix=/usr --enable-static --disable-unicode
make
make install DESTDIR=$SYSROOT
cd ..
rm -rf htop-$HTOP_VER

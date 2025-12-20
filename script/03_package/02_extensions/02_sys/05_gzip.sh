#!/bin/bash
set -e
source script/env.sh

if [ -f "$SYSROOT/bin/gzip" ]; then
    exit 0
fi

echo ">>> Building Gzip $GZIP_VER..."

mkdir -p $BUILD_DIR/tmp
cd $BUILD_DIR/tmp

export CROSS_COMPILE=$TARGET-
export CC=$TARGET-gcc
export CXX=$TARGET-g++
export AR=$TARGET-ar
export RANLIB=$TARGET-ranlib

tar -xf $SRC_DIR/gzip-$GZIP_VER.tar.xz
cd gzip-$GZIP_VER
./configure --host=$TARGET --prefix=/
make
make install DESTDIR=$SYSROOT
cd ..
rm -rf gzip-$GZIP_VER

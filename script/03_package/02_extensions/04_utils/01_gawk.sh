#!/bin/bash
set -e
source script/env.sh

if [ -f "$SYSROOT/usr/bin/gawk" ]; then
    exit 0
fi

echo ">>> Building Gawk $GAWK_VER..."

mkdir -p $BUILD_DIR/tmp
cd $BUILD_DIR/tmp

export CROSS_COMPILE=$TARGET-
export CC=$TARGET-gcc
export CXX=$TARGET-g++
export AR=$TARGET-ar
export RANLIB=$TARGET-ranlib

tar -xf $SRC_DIR/gawk-$GAWK_VER.tar.xz
cd gawk-$GAWK_VER
./configure --host=$TARGET --prefix=/usr
make
make install DESTDIR=$SYSROOT
cd ..
rm -rf gawk-$GAWK_VER

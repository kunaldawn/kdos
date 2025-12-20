#!/bin/bash
set -e
source script/env.sh

if [ -f "$SYSROOT/usr/bin/links" ]; then
    exit 0
fi

echo ">>> Building Links $LINKS_VER..."

mkdir -p $BUILD_DIR/tmp
cd $BUILD_DIR/tmp

export CROSS_COMPILE=$TARGET-
export CC=$TARGET-gcc
export CXX=$TARGET-g++
export AR=$TARGET-ar
export RANLIB=$TARGET-ranlib

tar -xf $SRC_DIR/links-$LINKS_VER.tar.gz
cd links-$LINKS_VER
./configure --host=$TARGET --prefix=/usr --with-ssl --enable-graphics=no
make
make install DESTDIR=$SYSROOT
cd ..
rm -rf links-$LINKS_VER

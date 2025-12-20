#!/bin/bash
set -e
source script/env.sh

if [ -f "$SYSROOT/usr/bin/file" ]; then
    exit 0
fi

echo ">>> Building File $FILE_VER..."

mkdir -p $BUILD_DIR/tmp
cd $BUILD_DIR/tmp

export CROSS_COMPILE=$TARGET-
export CC=$TARGET-gcc
export CXX=$TARGET-g++
export AR=$TARGET-ar
export RANLIB=$TARGET-ranlib

tar -xf $SRC_DIR/file-$FILE_VER.tar.gz
cd file-$FILE_VER
./configure --host=$TARGET --prefix=/usr
make
make install DESTDIR=$SYSROOT
cd ..
rm -rf file-$FILE_VER

#!/bin/bash
set -e
source script/env.sh

if [ -f "$SYSROOT/bin/nano" ]; then
    exit 0
fi

echo ">>> Building Nano $NANO_VER..."

mkdir -p $BUILD_DIR/tmp
cd $BUILD_DIR/tmp

export CROSS_COMPILE=$TARGET-
export CC=$TARGET-gcc
export CXX=$TARGET-g++
export AR=$TARGET-ar
export RANLIB=$TARGET-ranlib

tar -xf $SRC_DIR/nano-$NANO_VER.tar.xz
cd nano-$NANO_VER
./configure --host=$TARGET --prefix=/ --enable-static --disable-shared --enable-utf8 \
    --enable-color --enable-nanorc --enable-multibuffer
make
make install DESTDIR=$SYSROOT
cd ..
rm -rf nano-$NANO_VER

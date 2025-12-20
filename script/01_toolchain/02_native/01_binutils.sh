#!/bin/bash
set -e
source script/env.sh

if [ -f "$SYSROOT/usr/bin/ld" ]; then
    exit 0
fi

echo ">>> Building Native Binutils $BINUTILS_VER..."
mkdir -p $BUILD_DIR/tmp
cd $BUILD_DIR/tmp

export CROSS_COMPILE=$TARGET-
export CC=$TARGET-gcc
export CXX=$TARGET-g++

tar -xf $SRC_DIR/binutils-$BINUTILS_VER.tar.xz
cd binutils-$BINUTILS_VER
mkdir build && cd build
../configure --build=x86_64-pc-linux-gnu --host=$TARGET --target=$TARGET \
    --prefix=/usr --disable-nls --disable-werror
make
make install DESTDIR=$SYSROOT
cd ../..
rm -rf binutils-$BINUTILS_VER

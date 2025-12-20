#!/bin/bash
set -e
source script/env.sh

if [ -f "$SYSROOT/sbin/mkfs.vfat" ]; then
    exit 0
fi

echo ">>> Building Dosfstools $DOSFSTOOLS_VER..."

mkdir -p $BUILD_DIR/tmp
cd $BUILD_DIR/tmp

export CROSS_COMPILE=$TARGET-
export CC=$TARGET-gcc
export CXX=$TARGET-g++
export AR=$TARGET-ar
export RANLIB=$TARGET-ranlib

tar -xf $SRC_DIR/dosfstools-$DOSFSTOOLS_VER.tar.gz
cd dosfstools-$DOSFSTOOLS_VER
./configure --host=$TARGET --prefix= --enable-compat-symlinks --mandir=/usr/share/man --docdir=/usr/share/doc/dosfstools-$DOSFSTOOLS_VER
make
make install DESTDIR=$SYSROOT
cd ..
rm -rf dosfstools-$DOSFSTOOLS_VER

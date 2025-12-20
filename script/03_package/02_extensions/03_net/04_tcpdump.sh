#!/bin/bash
set -e
source script/env.sh

if [ -f "$SYSROOT/usr/sbin/tcpdump" ]; then
    exit 0
fi

echo ">>> Building Tcpdump $TCPDUMP_VER..."

mkdir -p $BUILD_DIR/tmp
cd $BUILD_DIR/tmp

export CROSS_COMPILE=$TARGET-
export CC=$TARGET-gcc
export CXX=$TARGET-g++
export AR=$TARGET-ar
export RANLIB=$TARGET-ranlib

tar -xf $SRC_DIR/tcpdump-$TCPDUMP_VER.tar.gz
cd tcpdump-$TCPDUMP_VER
./configure --host=$TARGET --prefix=/usr
make
make install DESTDIR=$SYSROOT
cd ..
rm -rf tcpdump-$TCPDUMP_VER

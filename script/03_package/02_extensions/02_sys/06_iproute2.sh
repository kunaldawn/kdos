#!/bin/bash
set -e
source script/env.sh

if [ -f "$SYSROOT/sbin/ip" ]; then
    exit 0
fi

echo ">>> Building Iproute2 $IPROUTE2_VER..."

mkdir -p $BUILD_DIR/tmp
cd $BUILD_DIR/tmp

export CROSS_COMPILE=$TARGET-
export CC=$TARGET-gcc
export CXX=$TARGET-g++
export AR=$TARGET-ar
export RANLIB=$TARGET-ranlib

tar -xf $SRC_DIR/iproute2-$IPROUTE2_VER.tar.xz
cd iproute2-$IPROUTE2_VER
# Point to kernel headers if needed, typically in $SYSROOT/usr/include
# Musl fixes: -D_GNU_SOURCE
make CC=$TARGET-gcc AR=$TARGET-ar CCOPTS="-O2 -pipe -I$SYSROOT/usr/include -D_GNU_SOURCE -DHAVE_SETNS -DHAVE_HANDLE_AT -include endian.h -include limits.h"
make install DESTDIR=$SYSROOT
cd ..
rm -rf iproute2-$IPROUTE2_VER

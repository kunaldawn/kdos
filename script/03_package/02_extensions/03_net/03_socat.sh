#!/bin/bash
set -e
source script/env.sh

if [ -f "$SYSROOT/usr/bin/socat" ]; then
    exit 0
fi

echo ">>> Building Socat $SOCAT_VER..."

mkdir -p $BUILD_DIR/tmp
cd $BUILD_DIR/tmp

export CROSS_COMPILE=$TARGET-
export CC=$TARGET-gcc
export CXX=$TARGET-g++
export AR=$TARGET-ar
export RANLIB=$TARGET-ranlib

tar -xf $SRC_DIR/socat-$SOCAT_VER.tar.gz
cd socat-$SOCAT_VER
./configure --host=$TARGET --prefix=/usr
# Fix strict C checks (GCC 15) and struct msghdr padding issues
# Also user requested single-threaded build for this tool
# IMPORTANT: Must include -D_GNU_SOURCE for sighandler_t in Musl
make -j1 CFLAGS="-O2 -pipe --sysroot=$SYSROOT -D_GNU_SOURCE -Wno-int-conversion -Wno-error=int-conversion"
make install DESTDIR=$SYSROOT
cd ..
rm -rf socat-$SOCAT_VER

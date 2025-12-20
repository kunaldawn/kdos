#!/bin/bash
set -e
source script/env.sh

if [ -f "$SYSROOT/usr/lib/libpcap.a" ]; then
    exit 0
fi

echo ">>> Building Libpcap $LIBPCAP_VER..."

mkdir -p $BUILD_DIR/tmp
cd $BUILD_DIR/tmp

export CROSS_COMPILE=$TARGET-
export CC=$TARGET-gcc
export CXX=$TARGET-g++
export AR=$TARGET-ar
export RANLIB=$TARGET-ranlib

tar -xf $SRC_DIR/libpcap-$LIBPCAP_VER.tar.gz
cd libpcap-$LIBPCAP_VER
./configure --host=$TARGET --prefix=/usr --disable-shared --with-pcap=linux
make
make install DESTDIR=$SYSROOT
cd ..
rm -rf libpcap-$LIBPCAP_VER

#!/bin/bash
set -e
source script/env.sh

if [ -f "$SYSROOT/usr/bin/curl" ]; then
    exit 0
fi

echo ">>> Building Curl $CURL_VER..."

mkdir -p $BUILD_DIR/tmp
cd $BUILD_DIR/tmp

export CROSS_COMPILE=$TARGET-
export CC=$TARGET-gcc
export CXX=$TARGET-g++
export AR=$TARGET-ar
export RANLIB=$TARGET-ranlib

tar -xf $SRC_DIR/curl-$CURL_VER.tar.gz
cd curl-$CURL_VER
./configure --host=$TARGET --prefix=/usr --with-openssl --with-zlib \
    --disable-shared --enable-static \
    --without-libpsl --without-libidn2 \
    PKG_CONFIG_PATH="$SYSROOT/usr/lib/pkgconfig" \
    LDFLAGS="-static"
make
make install DESTDIR=$SYSROOT
cd ..
rm -rf curl-$CURL_VER

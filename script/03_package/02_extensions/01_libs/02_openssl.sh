#!/bin/bash
set -e
source script/env.sh

if [ -f "$SYSROOT/usr/lib/libssl.a" ]; then
    exit 0
fi

echo ">>> Building OpenSSL $OPENSSL_VER..."

mkdir -p $BUILD_DIR/tmp
cd $BUILD_DIR/tmp

export CROSS_COMPILE=$TARGET-
export CC=$TARGET-gcc
export CXX=$TARGET-g++
export AR=$TARGET-ar
export RANLIB=$TARGET-ranlib

tar -xf $SRC_DIR/openssl-$OPENSSL_VER.tar.gz
cd openssl-$OPENSSL_VER

# OpenSSL double-prefixes if CROSS_COMPILE is set and CC is full path
unset CROSS_COMPILE

./Configure linux-x86_64 \
    --prefix=/usr --openssldir=/etc/ssl no-shared -fPIC
make
make install DESTDIR=$SYSROOT

cd ..
rm -rf openssl-$OPENSSL_VER

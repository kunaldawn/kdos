#!/bin/bash
set -e
source script/env.sh

if [ -f "$SYSROOT/usr/bin/jq" ]; then
    exit 0
fi

echo ">>> Building Jq $JQ_VER..."

mkdir -p $BUILD_DIR/tmp
cd $BUILD_DIR/tmp

export CROSS_COMPILE=$TARGET-
export CC=$TARGET-gcc
export CXX=$TARGET-g++
export AR=$TARGET-ar
export RANLIB=$TARGET-ranlib

tar -xf $SRC_DIR/jq-$JQ_VER.tar.gz
cd jq-$JQ_VER
./configure --host=$TARGET --prefix=/usr --with-oniguruma=builtin --disable-maintainer-mode
make
make install DESTDIR=$SYSROOT
cd ..
rm -rf jq-$JQ_VER

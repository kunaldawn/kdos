#!/bin/bash
set -e
source script/env.sh

if [ -f "$SYSROOT/usr/lib/libreadline.a" ]; then
    exit 0
fi

echo ">>> Building Readline $READLINE_VER..."

mkdir -p $BUILD_DIR/tmp
cd $BUILD_DIR/tmp

export CROSS_COMPILE=$TARGET-
export CC=$TARGET-gcc
export CXX=$TARGET-g++
export AR=$TARGET-ar
export RANLIB=$TARGET-ranlib

tar -xf $SRC_DIR/readline-$READLINE_VER.tar.gz
cd readline-$READLINE_VER
./configure --host=$TARGET --prefix=/usr --disable-shared
make
make install DESTDIR=$SYSROOT
cd ..
rm -rf readline-$READLINE_VER

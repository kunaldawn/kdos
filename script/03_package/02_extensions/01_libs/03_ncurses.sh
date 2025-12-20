#!/bin/bash
set -e
source script/env.sh

if [ -f "$SYSROOT/usr/lib/libncurses.a" ]; then
    exit 0
fi

echo ">>> Building Ncurses $NCURSES_VER..."

mkdir -p $BUILD_DIR/tmp
cd $BUILD_DIR/tmp

export CROSS_COMPILE=$TARGET-
export CC=$TARGET-gcc
export CXX=$TARGET-g++
export AR=$TARGET-ar
export RANLIB=$TARGET-ranlib

tar -xf $SRC_DIR/ncurses-$NCURSES_VER.tar.gz
cd ncurses-$NCURSES_VER
./configure --host=$TARGET --prefix=/usr --enable-widec --without-debug \
    --without-shared --without-ada --without-manpages --without-tests --with-normal \
    --without-cxx-binding --without-cxx
make
make install DESTDIR=$SYSROOT
cd ..
rm -rf ncurses-$NCURSES_VER

# Fix: Create symlinks for non-wide calls (needed by many tools)
ln -sf libncursesw.a $SYSROOT/usr/lib/libncurses.a
ln -sf libncursesw.a $SYSROOT/usr/lib/libcurses.a

#!/bin/bash
set -e
source script/env.sh

if [ -f "$SYSROOT/usr/bin/tmux" ]; then
    exit 0
fi

echo ">>> Building Tmux $TMUX_VER..."

mkdir -p $BUILD_DIR/tmp
cd $BUILD_DIR/tmp

export CROSS_COMPILE=$TARGET-
export CC=$TARGET-gcc
export CXX=$TARGET-g++
export AR=$TARGET-ar
export RANLIB=$TARGET-ranlib

tar -xf $SRC_DIR/tmux-$TMUX_VER.tar.gz
cd tmux-$TMUX_VER
./configure --host=$TARGET --prefix=/usr --enable-static LIBEVENT_CFLAGS="-I$SYSROOT/usr/include" LIBEVENT_LIBS="-L$SYSROOT/usr/lib -levent"
make
make install DESTDIR=$SYSROOT
cd ..
rm -rf tmux-$TMUX_VER

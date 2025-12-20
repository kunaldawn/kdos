#!/bin/bash
set -e
source script/env.sh

# Toybox may have created a symlink for bash (if configured to do so).
# We want the REAL bash, so remove the symlink if it exists.
if [ -L "$SYSROOT/bin/bash" ]; then
    rm -f "$SYSROOT/bin/bash"
fi

if [ -f "$SYSROOT/bin/bash" ]; then
    exit 0
fi

echo ">>> Building Bash $BASH_VER..."

mkdir -p $BUILD_DIR/tmp
cd $BUILD_DIR/tmp

export CROSS_COMPILE=$TARGET-
export CC=$TARGET-gcc
export CXX=$TARGET-g++
export AR=$TARGET-ar
export RANLIB=$TARGET-ranlib

tar -xf $SRC_DIR/bash-$BASH_VER.tar.gz
cd bash-$BASH_VER
./configure --host=$TARGET --prefix=/ --without-bash-malloc --enable-static-link
make
make install DESTDIR=$SYSROOT
cd ..
rm -rf bash-$BASH_VER

ln -sf bash "$SYSROOT/bin/sh"

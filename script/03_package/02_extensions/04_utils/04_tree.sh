#!/bin/bash
set -e
source script/env.sh

if [ -f "$SYSROOT/usr/bin/tree" ]; then
    exit 0
fi

echo ">>> Building Tree $TREE_VER..."

mkdir -p $BUILD_DIR/tmp
cd $BUILD_DIR/tmp

export CROSS_COMPILE=$TARGET-
export CC=$TARGET-gcc
export CXX=$TARGET-g++
export AR=$TARGET-ar
export RANLIB=$TARGET-ranlib

tar -xf $SRC_DIR/tree-$TREE_VER.tar.gz
cd unix-tree-$TREE_VER
# Tree typically has no configure script
make CC=$TARGET-gcc
make install PREFIX=$SYSROOT/usr
cd ..
rm -rf unix-tree-$TREE_VER

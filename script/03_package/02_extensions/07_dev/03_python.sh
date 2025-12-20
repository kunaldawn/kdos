#!/bin/bash
set -e
source script/env.sh

if [ -f "$SYSROOT/usr/bin/python3" ]; then
    exit 0
fi

echo ">>> Building Python $PYTHON_VER..."

mkdir -p $BUILD_DIR/tmp
cd $BUILD_DIR/tmp

export CROSS_COMPILE=$TARGET-
export CC=$TARGET-gcc
export CXX=$TARGET-g++
export AR=$TARGET-ar
export RANLIB=$TARGET-ranlib

tar -xf $SRC_DIR/Python-$PYTHON_VER.tar.xz
cd Python-$PYTHON_VER

# 1. Host Build (for cross-compilation tools)
mkdir -p host-build
cd host-build
../configure
make
cd ..

# 2. Target Build (using host python)
mkdir -p target-build
cd target-build
export ac_cv_file__dev_ptmx=yes
export ac_cv_file__dev_ptc=no
../configure --host=$TARGET --build=x86_64-linux-gnu --prefix=/usr --disable-ipv6 --with-build-python=$(pwd)/../host-build/python
make
make install DESTDIR=$SYSROOT
cd ../..
rm -rf Python-$PYTHON_VER

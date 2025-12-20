#!/bin/bash
set -e
source script/env.sh

if [ -f "$SYSROOT/sbin/mkfs.ext4" ]; then
    exit 0
fi

echo ">>> Building E2fsprogs $E2FSPROGS_VER..."

mkdir -p $BUILD_DIR/tmp
cd $BUILD_DIR/tmp

export CROSS_COMPILE=$TARGET-
export CC=$TARGET-gcc
export CXX=$TARGET-g++
export AR=$TARGET-ar
export RANLIB=$TARGET-ranlib

tar -xf $SRC_DIR/e2fsprogs-$E2FSPROGS_VER.tar.xz
cd e2fsprogs-$E2FSPROGS_VER
mkdir build && cd build
../configure --host=$TARGET --prefix=/usr --with-root-prefix="" --enable-elf-shlibs --disable-libblkid --disable-libuuid --disable-uuidd --disable-fsck
make
make install DESTDIR=$SYSROOT
cd ../..
rm -rf e2fsprogs-$E2FSPROGS_VER

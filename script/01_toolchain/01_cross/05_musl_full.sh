#!/bin/bash
set -e
source script/env.sh

if [ -f "$SYSROOT/usr/lib/libc.a" ]; then
    exit 0
fi

echo ">>> Building Musl $MUSL_VER (Shared/Static)..."
mkdir -p $BUILD_DIR/tmp
cd $BUILD_DIR/tmp

tar -xf $SRC_DIR/musl-$MUSL_VER.tar.gz
cd musl-$MUSL_VER

# We use the Phase 1 GCC here
export CROSS_COMPILE=$TARGET-
export CC=$TARGET-gcc

./configure --prefix=/usr --syslibdir=/lib
make
make install DESTDIR=$SYSROOT

# Fix: Move libc.so to /lib (critical for runtime loader)
mkdir -p $SYSROOT/lib
if [ -f "$SYSROOT/usr/lib/libc.so" ]; then
    mv $SYSROOT/usr/lib/libc.so $SYSROOT/lib/libc.so
fi

# Create dynamic linker symlink (Runtime)
# The loader is just a symlink to libc.so in Musl
ln -sf libc.so $SYSROOT/lib/ld-musl-x86_64.so.1

# Create compat symlink for linker (Build-time)
# GCC looks in /usr/lib, so we need libc.so there pointing to the real one
# Use relative path to avoid "host linkage" confusion
ln -sf ../../lib/libc.so $SYSROOT/usr/lib/libc.so
cd ..
rm -rf musl-$MUSL_VER

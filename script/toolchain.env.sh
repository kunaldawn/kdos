#!/bin/bash
# Environment configuration for KDOS build

export KDOS_TARGET=x86_64-kdos-linux-musl

export WORKSPACE=/workspace
export BUILD_DIR=$WORKSPACE/build
export SYSROOT=$BUILD_DIR/fs
export CROSS_SYSROOT=$BUILD_DIR/cross
export MARK=$BUILD_DIR/mark/toolchain

# Helpers
mkdir -p $BUILD_DIR $SYSROOT $CROSS_SYSROOT $MARK
rm -rf $BUILD_DIR/tmp
mkdir -p $BUILD_DIR/tmp

export PKG_CONFIG_PATH=""
export PKG_CONFIG_LIBDIR=$SYSROOT/usr/lib/pkgconfig:$SYSROOT/usr/share/pkgconfig
export PKG_CONFIG_SYSROOT_DIR=$SYSROOT

export PATH=$CROSS_SYSROOT/bin:$CROSS_SYSROOT/usr/bin:$PATH

export CFLAGS="-O2 -pipe -std=gnu99"
export CXXFLAGS="-O2 -pipe"
export LDFLAGS=""
export MAKEFLAGS="-j12"

echo "Toolchain ENV is active..."

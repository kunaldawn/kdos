#!/bin/bash
set -e
source script/env.sh

echo ">>> Building Toybox $TOYBOX_VER..."

mkdir -p $BUILD_DIR/tmp
cd $BUILD_DIR/tmp

export CROSS_COMPILE=$TARGET-
export CC=$TARGET-gcc
export CXX=$TARGET-g++
export AR=$TARGET-ar

# Toybox
tar -xf $SRC_DIR/toybox-$TOYBOX_VER.tar.gz
cd toybox-$TOYBOX_VER

# Toybox expects 'cc'. Create symlink if missing.
if [ ! -f "$CROSS_DIR/bin/$TARGET-cc" ]; then
    ln -sf $TARGET-gcc $CROSS_DIR/bin/$TARGET-cc
fi

# Unset ALL toolchain variables so Toybox uses CROSS_COMPILE + (cc|strip|etc)
# This prevents double-prefixing (e.g. x86_64-kdos-...-x86_64-kdos-...-strip)
unset CC CXX CFLAGS CXXFLAGS LDFLAGS AR AS LD RANLIB STRIP OBJCOPY
export HOSTCC=gcc

if [ -f "$WORKSPACE/src/config/.config.toybox" ]; then
    cp "$WORKSPACE/src/config/.config.toybox" .config
else
    make -j1 defconfig
fi

# Clean previous install artifacts to avoid Permission Denied on stale files
rm -f $SYSROOT/bin/toybox
rm -f $SYSROOT/bin/toybox-x86_64-kdos-linux-musl

PREFIX=$SYSROOT make -j1 install

# Fix renaming if cross-compile suffix is used
if [ -f "$SYSROOT/bin/toybox-x86_64-kdos-linux-musl" ]; then
    mv "$SYSROOT/bin/toybox-x86_64-kdos-linux-musl" "$SYSROOT/bin/toybox"
    # Create a link for the long name so that all other symlinks (mount, ls, etc) remain valid
    ln -sf toybox "$SYSROOT/bin/toybox-x86_64-kdos-linux-musl"
fi

cd ..
rm -rf toybox-$TOYBOX_VER

# Ensure it exists
if [ ! -f "$SYSROOT/bin/toybox" ]; then
        echo "ERROR: Toybox install failed to create $SYSROOT/bin/toybox"
        exit 1
fi

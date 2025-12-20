#!/bin/bash
set -e
source script/env.sh

# Build using HOST compiler
unset CC CXX CFLAGS CXXFLAGS LDFLAGS AR AS LD RANLIB STRIP OBJCOPY PKG_CONFIG_PATH

if [ -f "$CROSS_DIR/bin/$TARGET-ld" ]; then
    exit 0
fi

echo ">>> Building Binutils $BINUTILS_VER..."
mkdir -p $CROSS_DIR
mkdir -p $BUILD_DIR/tmp
cd $BUILD_DIR/tmp

tar -xf $SRC_DIR/binutils-$BINUTILS_VER.tar.xz
cd binutils-$BINUTILS_VER
mkdir build && cd build
../configure --target=$TARGET --prefix=$CROSS_DIR --with-sysroot=$SYSROOT \
    --disable-nls --disable-werror --disable-multilib
make
make install
cd ../..
rm -rf binutils-$BINUTILS_VER

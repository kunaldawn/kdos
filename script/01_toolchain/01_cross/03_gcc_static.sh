#!/bin/bash
set -e
source script/env.sh

# Build using HOST compiler
unset CC CXX CFLAGS CXXFLAGS LDFLAGS AR AS LD RANLIB STRIP OBJCOPY PKG_CONFIG_PATH

if [ -f "$CROSS_DIR/bin/$TARGET-gcc-initial" ]; then
    exit 0
fi

echo ">>> Building GCC $GCC_VER (Phase 1)..."
mkdir -p $BUILD_DIR/tmp
cd $BUILD_DIR/tmp

tar -xf $SRC_DIR/gcc-$GCC_VER.tar.xz
cd gcc-$GCC_VER
tar -xf $SRC_DIR/gmp-$GMP_VER.tar.xz
mv gmp-$GMP_VER gmp
tar -xf $SRC_DIR/mpfr-$MPFR_VER.tar.xz
mv mpfr-$MPFR_VER mpfr
tar -xf $SRC_DIR/mpc-$MPC_VER.tar.gz
mv mpc-$MPC_VER mpc

mkdir build && cd build
../configure --target=$TARGET --prefix=$CROSS_DIR --with-sysroot=$SYSROOT \
    --without-headers --with-newlib \
    --enable-languages=c \
    --disable-nls --disable-shared --disable-multilib \
    --disable-threads --disable-libatomic --disable-libgomp \
    --disable-libquadmath --disable-libssp --disable-libvtv \
    --disable-libstdcxx --disable-bootstrap
make all-gcc all-target-libgcc
make install-gcc install-target-libgcc
cd ../..
# Rename to avoid confusion with final gcc
mv $CROSS_DIR/bin/$TARGET-gcc $CROSS_DIR/bin/$TARGET-gcc-initial
rm -rf gcc-$GCC_VER

# Symlink back so musl build finds it as "gcc"
ln -s $TARGET-gcc-initial $CROSS_DIR/bin/$TARGET-gcc

#!/bin/bash
set -e
source script/env.sh

if [ -f "$SYSROOT/usr/bin/gcc" ]; then
    exit 0
fi

echo ">>> Building Native GCC $GCC_VER..."
mkdir -p $BUILD_DIR/tmp
cd $BUILD_DIR/tmp

export CROSS_COMPILE=$TARGET-
export CC=$TARGET-gcc
export CXX=$TARGET-g++

tar -xf $SRC_DIR/gcc-$GCC_VER.tar.xz
cd gcc-$GCC_VER
tar -xf $SRC_DIR/gmp-$GMP_VER.tar.xz
mv gmp-$GMP_VER gmp
tar -xf $SRC_DIR/mpfr-$MPFR_VER.tar.xz
mv mpfr-$MPFR_VER mpfr
tar -xf $SRC_DIR/mpc-$MPC_VER.tar.gz
mv mpc-$MPC_VER mpc

mkdir build && cd build
../configure --build=x86_64-pc-linux-gnu --host=$TARGET --target=$TARGET \
    --prefix=/usr \
    --enable-languages=c,c++ \
    --disable-multilib --disable-nls --disable-bootstrap \
    --with-sysroot=/
make
make install DESTDIR=$SYSROOT
cd ../..
rm -rf gcc-$GCC_VER

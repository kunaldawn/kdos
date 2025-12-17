#!/bin/bash
set -e
source script/env.sh

echo ">>> Building Native Toolchain (Self-Hosting)..."

mkdir -p $BUILD_DIR/tmp
cd $BUILD_DIR/tmp

export CROSS_COMPILE=$TARGET-
export CC=$TARGET-gcc
export CXX=$TARGET-g++

# 1. Native Binutils
if [ ! -f "$SYSROOT/usr/bin/ld" ]; then
    echo ">>> Building Native Binutils $BINUTILS_VER..."
    tar -xf $SRC_DIR/binutils-$BINUTILS_VER.tar.xz
    cd binutils-$BINUTILS_VER
    mkdir build && cd build
    ../configure --build=x86_64-pc-linux-gnu --host=$TARGET --target=$TARGET \
        --prefix=/usr --disable-nls --disable-werror
    make
    make install DESTDIR=$SYSROOT
    cd ../..
    rm -rf binutils-$BINUTILS_VER
fi

# 2. Native GCC
if [ ! -f "$SYSROOT/usr/bin/gcc" ]; then
    echo ">>> Building Native GCC $GCC_VER..."
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
fi

# 3. Native Make
if [ ! -f "$SYSROOT/usr/bin/make" ]; then
    echo ">>> Building Native Make $MAKE_VER..."
    tar -xf $SRC_DIR/make-$MAKE_VER.tar.gz
    cd make-$MAKE_VER
    ./configure --host=$TARGET --prefix=/usr --disable-nls \
        CFLAGS="-std=gnu99 -O2 -pipe --sysroot=$SYSROOT"
    make
    make install DESTDIR=$SYSROOT
    cd ..
    rm -rf make-$MAKE_VER
fi

echo ">>> Native Toolchain Installed."

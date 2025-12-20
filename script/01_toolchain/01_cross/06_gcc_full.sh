#!/bin/bash
set -e
source script/env.sh

# Build using HOST compiler
unset CC CXX CFLAGS CXXFLAGS LDFLAGS AR AS LD RANLIB STRIP OBJCOPY PKG_CONFIG_PATH

if [ -f "$CROSS_DIR/bin/$TARGET-g++" ]; then
    exit 0
fi

echo ">>> Building GCC $GCC_VER (Phase 2 - Full)..."
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

# We remove the "newlib" stuff and enable threads/shared if desired.
# We are statically linking most target binaries, but the toolchain itself
# should support standard C/C++.
../configure --target=$TARGET --prefix=$CROSS_DIR --with-sysroot=$SYSROOT \
    --enable-languages=c,c++ \
    --disable-nls --disable-multilib --disable-bootstrap \
    --disable-libsanitizer \
    --with-native-system-header-dir=/usr/include
    
make
make install
cd ../..
rm -rf gcc-$GCC_VER

# --- Final Polish: Copy Runtime Libraries ---
# Essential for dynamic C/C++ binaries on the target (like the native compiler)
echo ">>> Installing Runtime Libraries (libgcc_s, libstdc++)..."

# Use the compiler to find the true location of the libs
LIBGCC_PATH=$($CROSS_DIR/bin/$TARGET-gcc -print-file-name=libgcc_s.so.1)
LIBSTDCPP_PATH=$($CROSS_DIR/bin/$TARGET-g++ -print-file-name=libstdc++.so)

# Resolve symlink directory to get the real files
LIBGCC_DIR=$(dirname $(readlink -f $LIBGCC_PATH))
LIBSTD_DIR=$(dirname $(readlink -f $LIBSTDCPP_PATH))

# Copy them to /usr/lib
cp -d "$LIBGCC_DIR"/libgcc_s.so* "$SYSROOT/usr/lib/" || echo "WARNING: libgcc_s copy failed"
cp -d "$LIBSTD_DIR"/libstdc++.so* "$SYSROOT/usr/lib/" || echo "WARNING: libstdc++ copy failed"

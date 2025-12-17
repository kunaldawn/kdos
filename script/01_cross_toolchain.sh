#!/bin/bash
set -e
source script/env.sh

# CRITICAL: We are building the toolchain itself.
# We must use the HOST compiler (gcc), not the TARGET compiler (which doesn't exist yet).
unset CC CXX CFLAGS CXXFLAGS LDFLAGS AR AS LD RANLIB STRIP OBJCOPY PKG_CONFIG_PATH

echo ">>> Building Cross-Toolchain ($TARGET)..."

mkdir -p $CROSS_DIR
mkdir -p $BUILD_DIR/tmp
cd $BUILD_DIR/tmp

# --- 1. Binutils ---
if [ ! -f "$CROSS_DIR/bin/$TARGET-ld" ]; then
    echo ">>> Building Binutils $BINUTILS_VER..."
    tar -xf $SRC_DIR/binutils-$BINUTILS_VER.tar.xz
    cd binutils-$BINUTILS_VER
    mkdir build && cd build
    ../configure --target=$TARGET --prefix=$CROSS_DIR --with-sysroot=$SYSROOT \
        --disable-nls --disable-werror --disable-multilib
    make
    make install
    cd ../..
    rm -rf binutils-$BINUTILS_VER
fi

# --- 2. Linux Headers ---
# Headers are needed for the toolchain and target libs
if [ ! -f "$SYSROOT/usr/include/linux/version.h" ]; then
    echo ">>> Installing Linux Headers $LINUX_VER..."
    mkdir -p $SYSROOT/usr/include
    tar -xf $SRC_DIR/linux-$LINUX_VER.tar.xz
    cd linux-$LINUX_VER
    make ARCH=x86_64 headers_install INSTALL_HDR_PATH=$SYSROOT/usr
    cd ..
    rm -rf linux-$LINUX_VER
fi

# --- 3. GCC Phase 1 (Core C) ---
# We build a static GCC first to compile Musl.
if [ ! -f "$CROSS_DIR/bin/$TARGET-gcc-initial" ]; then
    echo ">>> Building GCC $GCC_VER (Phase 1)..."
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
fi

# --- 4. Musl Headers ---
if [ ! -f "$CROSS_DIR/$TARGET/usr/include/unistd.h" ]; then
    echo ">>> Installing Musl Headers $MUSL_VER..."
    tar -xf $SRC_DIR/musl-$MUSL_VER.tar.gz
    cd musl-$MUSL_VER
    # We install headers to sysroot. Prefix /usr so they go to /usr/include
    ./configure --prefix=/usr --syslibdir=/lib
    make install-headers DESTDIR=$SYSROOT
    cd ..
    rm -rf musl-$MUSL_VER
fi

# --- 5. Musl (Static) ---
if [ ! -f "$SYSROOT/usr/lib/libc.a" ]; then
    echo ">>> Building Musl $MUSL_VER (Shared/Static)..."
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
    unset CC
    unset CROSS_COMPILE
fi

# --- 6. GCC Phase 2 (Full C/C++) ---
# Now that we have libc, we build the full compiler.
if [ ! -f "$CROSS_DIR/bin/$TARGET-g++" ]; then
    echo ">>> Building GCC $GCC_VER (Phase 2 - Full)..."
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
    
    # --- 7. Final Polish: Copy Runtime Libraries ---
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
fi

echo ">>> Cross-Toolchain Ready."

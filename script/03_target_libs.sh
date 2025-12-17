#!/bin/bash
set -e
source script/env.sh

echo ">>> Building Target Libraries..."

mkdir -p $BUILD_DIR/tmp
cd $BUILD_DIR/tmp

export CROSS_COMPILE=$TARGET-
export CC=$TARGET-gcc
export CXX=$TARGET-g++
export AR=$TARGET-ar
export RANLIB=$TARGET-ranlib

# 1. Zlib
if [ ! -f "$SYSROOT/usr/lib/libz.a" ]; then
    echo ">>> Building Zlib $ZLIB_VER..."
    tar -xf $SRC_DIR/zlib-$ZLIB_VER.tar.gz
    cd zlib-$ZLIB_VER
    ./configure --prefix=/usr --static
    make
    make install DESTDIR=$SYSROOT
    cd ..
    rm -rf zlib-$ZLIB_VER
fi

# 1.5 Musl-FTS (Required for Nnn/Void tools)
if [ ! -f "$SYSROOT/usr/lib/libfts.a" ]; then
    echo ">>> Building Musl-FTS $MUSL_FTS_VER..."
    tar -xf $SRC_DIR/musl-fts-$MUSL_FTS_VER.tar.gz
    cd musl-fts-$MUSL_FTS_VER
    ./bootstrap.sh
    ./configure --prefix=/usr --sysconfdir=/etc --localstatedir=/var --disable-shared
    make
    make install DESTDIR=$SYSROOT
    cd ..
    rm -rf musl-fts-$MUSL_FTS_VER
    
    # Generate pkg-config file if missing (simple static linkage)
    mkdir -p $SYSROOT/usr/lib/pkgconfig
    cat > $SYSROOT/usr/lib/pkgconfig/musl-fts.pc <<EOF
prefix=/usr
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: musl-fts
Description: Implementation of fts(3) for musl libc
Version: $MUSL_FTS_VER
Libs: -L\${libdir} -lfts
Cflags: -I\${includedir}
EOF
fi

# 2. OpenSSL
if [ ! -f "$SYSROOT/usr/lib/libssl.a" ]; then
    echo ">>> Building OpenSSL $OPENSSL_VER..."
    tar -xf $SRC_DIR/openssl-$OPENSSL_VER.tar.gz
    cd openssl-$OPENSSL_VER
    
    # OpenSSL double-prefixes if CROSS_COMPILE is set and CC is full path
    unset CROSS_COMPILE
    
    ./Configure linux-x86_64 \
        --prefix=/usr --openssldir=/etc/ssl no-shared
    make
    make install DESTDIR=$SYSROOT
    
    # Restore for next steps
    export CROSS_COMPILE=$TARGET-
    
    cd ..
    rm -rf openssl-$OPENSSL_VER
fi

# 3. Ncurses
if [ ! -f "$SYSROOT/usr/lib/libncurses.a" ]; then
    echo ">>> Building Ncurses $NCURSES_VER..."
    tar -xf $SRC_DIR/ncurses-$NCURSES_VER.tar.gz
    cd ncurses-$NCURSES_VER
    ./configure --host=$TARGET --prefix=/usr --enable-widec --without-debug \
        --without-shared --without-ada --without-manpages --without-tests --with-normal \
        --without-cxx-binding --without-cxx
    make
    make install DESTDIR=$SYSROOT
    cd ..
    rm -rf ncurses-$NCURSES_VER
    
    # Fix: Create symlinks for non-wide calls (needed by many tools)
    ln -sf libncursesw.a $SYSROOT/usr/lib/libncurses.a
    ln -sf libncursesw.a $SYSROOT/usr/lib/libcurses.a
fi

# 4. Readline
if [ ! -f "$SYSROOT/usr/lib/libreadline.a" ]; then
    echo ">>> Building Readline $READLINE_VER..."
    tar -xf $SRC_DIR/readline-$READLINE_VER.tar.gz
    cd readline-$READLINE_VER
    ./configure --host=$TARGET --prefix=/usr --disable-shared
    make
    make install DESTDIR=$SYSROOT
    cd ..
    rm -rf readline-$READLINE_VER
fi

# 5. Libevent
if [ ! -f "$SYSROOT/usr/lib/libevent.a" ]; then
    echo ">>> Building Libevent $LIBEVENT_VER..."
    tar -xf $SRC_DIR/libevent-$LIBEVENT_VER.tar.gz
    cd libevent-$LIBEVENT_VER
    ./configure --host=$TARGET --prefix=/usr --disable-shared --disable-openssl
    make
    make install DESTDIR=$SYSROOT
    cd ..
    rm -rf libevent-$LIBEVENT_VER
fi

# 6. Libpcap
if [ ! -f "$SYSROOT/usr/lib/libpcap.a" ]; then
    echo ">>> Building Libpcap $LIBPCAP_VER..."
    tar -xf $SRC_DIR/libpcap-$LIBPCAP_VER.tar.gz
    cd libpcap-$LIBPCAP_VER
    ./configure --host=$TARGET --prefix=/usr --disable-shared --with-pcap=linux
    make
    make install DESTDIR=$SYSROOT
    cd ..
    rm -rf libpcap-$LIBPCAP_VER
fi

echo ">>> Target Libraries Built."

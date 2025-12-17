#!/bin/bash
# Environment configuration for KDOS build

export TARGET=x86_64-kdos-linux-musl

# Directories
if [ -d "/workspace" ]; then
    export WORKSPACE=/workspace
else
    export WORKSPACE=$(pwd)
fi

export BUILD_DIR=$WORKSPACE/build
export CROSS_DIR=$BUILD_DIR/cross
export SYSROOT=$BUILD_DIR/fs
export SRC_DIR=$WORKSPACE/src
export SCRIPT_DIR=$WORKSPACE/script

# Versions
export LINUX_VER=6.18.1
export MUSL_VER=1.2.5
export TOYBOX_VER=0.8.13
export BASH_VER=5.3
export BINUTILS_VER=2.45.1
export GCC_VER=15.2.0
export MAKE_VER=4.4.1
export GMP_VER=6.3.0
export MPFR_VER=4.2.2
export MPC_VER=1.3.1
export ZLIB_VER=1.3.1
export OPENSSL_VER=3.6.0
export MUSL_FTS_VER=1.2.7
export NCURSES_VER=6.5
export READLINE_VER=8.3
export LIBEVENT_VER=2.1.12-stable
export LIBPCAP_VER=1.10.5
export DROPBEAR_VER=2025.88
export CURL_VER=8.17.0
export SOCAT_VER=1.8.1.0
export TCPDUMP_VER=4.99.5
export LINKS_VER=2.30
export HTOP_VER=3.4.1
export BC_VER=1.08.2
export JQ_VER=1.8.1
export VIM_VER=9.1
export TMUX_VER=3.6a
export NANO_VER=8.7

# Mirrors
export GNU_MIRROR="https://mirrors.hopbox.net/gnu"

# Helpers
mkdir -p $BUILD_DIR $CROSS_DIR $SYSROOT $SRC_DIR

export PATH=$CROSS_DIR/bin:$PATH
export CROSS_COMPILE=$TARGET-

export CC=$TARGET-gcc
export CXX=$TARGET-g++
export AR=$TARGET-ar
export AS=$TARGET-as
export LD=$TARGET-ld
export RANLIB=$TARGET-ranlib
export STRIP=$TARGET-strip
export OBJCOPY=$TARGET-objcopy

# Pkg Configuration for Cross-Compile
export PKG_CONFIG_PATH=""
export PKG_CONFIG_LIBDIR=$SYSROOT/usr/lib/pkgconfig:$SYSROOT/usr/share/pkgconfig
export PKG_CONFIG_SYSROOT_DIR=$SYSROOT

export CFLAGS="-O2 -pipe --sysroot=$SYSROOT"
export CXXFLAGS="-O2 -pipe --sysroot=$SYSROOT"
export LDFLAGS="--sysroot=$SYSROOT"

# Use nproc - 2 to allow responsiveness, min 1
CORES=$(nproc)
if [ "$CORES" -gt 2 ]; then
    CORES=$((CORES - 2))
else
    CORES=1
fi
export MAKEFLAGS="-j$CORES"

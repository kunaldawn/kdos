#!/bin/bash

# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# Environment configuration for KDOS build

# --- build-system metadata (PARSED by the orchestrator, never sourced) ---
export KDOS_PHASE_TITLE="Base Userland"
export KDOS_PHASE_DESC="musl, toybox, bash, native gcc, kpkg, kinstall"
export KDOS_SNAPSHOT_PATHS="cross fs mark"
export KDOS_SNAPSHOT_EXCLUDE="fs/tmp/* fs/dev/* fs/proc/* fs/sys/* fs/run/* fs/kdos/* fs/ports/*"

export KDOS_TARGET=x86_64-kdos-linux-musl

export WORKSPACE=/workspace
export BUILD_DIR=$WORKSPACE/build
export SYSROOT=$BUILD_DIR/fs
export CROSS_SYSROOT=$BUILD_DIR/cross
export MARK=$BUILD_DIR/mark/phase1

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

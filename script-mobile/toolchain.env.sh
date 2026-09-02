#!/bin/bash

# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro — mobile
# ---------------------------------

# Environment configuration for the KDOS mobile build

# --- build-system metadata (PARSED by the orchestrator, never sourced) ---
export KDOS_PHASE_TITLE="Cross Toolchain"
export KDOS_PHASE_DESC="cross binutils + gcc targeting aarch64-kdos-linux-musl"
export KDOS_SNAPSHOT_PATHS="cross fs mark"
export KDOS_SNAPSHOT_EXCLUDE="fs/tmp/* fs/dev/* fs/proc/* fs/sys/* fs/run/* fs/kdos/* fs/ports/*"

export KDOS_TARGET=aarch64-kdos-linux-musl

# WHICH PHONE. Boards share phases 00-04 and their snapshots; only the kernel
# phase and packaging read this. It is defaulted here and forwarded through
# chroot_exec.sh, so a step that reads it sees the same value on both sides.
export KDOS_BOARD=${KDOS_BOARD:-fajita}

export WORKSPACE=/workspace
export BUILD_DIR=$WORKSPACE/build-mobile
export SYSROOT=$BUILD_DIR/fs
export CROSS_SYSROOT=$BUILD_DIR/cross
export MARK=$BUILD_DIR/mark/toolchain

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

# Reproducible packages: the same five lines as the desktop target, for the
# same reasons — see script/toolchain.env.sh, which states them once.
export SOURCE_DATE_EPOCH=1735689600
export TZ=UTC
export LC_ALL=C
export CFLAGS="$CFLAGS -ffile-prefix-map=/var/cache/kpkg/work=/build"
export CXXFLAGS="$CXXFLAGS -ffile-prefix-map=/var/cache/kpkg/work=/build"
export LDFLAGS="$LDFLAGS -Wl,--build-id=sha1"
export MAKEFLAGS="-j12"

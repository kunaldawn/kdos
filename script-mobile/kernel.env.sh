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

# --- build-system metadata (PARSED by the orchestrator, never sourced) ---
export KDOS_PHASE_TITLE="Kernel"
export KDOS_PHASE_DESC="cross-build the board's kernel, DTB and modules"
export KDOS_SNAPSHOT_PATHS="fs mark"
export KDOS_SNAPSHOT_EXCLUDE="fs/tmp/* fs/dev/* fs/proc/* fs/sys/* fs/run/* fs/kdos/* fs/ports/*"

# NO CHROOT. The kernel is the one large build that gains nothing from running
# under emulation — it is cross-compiled anyway, so the chroot would buy an
# interpreter and cost a factor of five.
export KDOS_TARGET=aarch64-kdos-linux-musl
export KDOS_BOARD=${KDOS_BOARD:-fajita}

export WORKSPACE=/workspace
export BUILD_DIR=$WORKSPACE/build-mobile
export SYSROOT=$BUILD_DIR/fs
export CROSS_SYSROOT=$BUILD_DIR/cross
export MARK=$BUILD_DIR/mark/kernel

mkdir -p $MARK

export PATH=$CROSS_SYSROOT/bin:$CROSS_SYSROOT/usr/bin:$PATH

export SOURCE_DATE_EPOCH=1735689600
export TZ=UTC
export LC_ALL=C
export MAKEFLAGS="-j12"

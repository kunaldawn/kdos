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
export KDOS_PHASE_TITLE="Packaging"
export KDOS_PHASE_DESC="trim the rootfs, install firmware, roll rootfs.ext4 and boot.img"
export KDOS_SNAPSHOT_PATHS="fs images"
export KDOS_SNAPSHOT_EXCLUDE="fs/tmp/* fs/var/cache/kpkg/work/* fs/dev/* fs/proc/* fs/sys/* fs/run/* fs/kdos/* fs/ports/*"

# NO CHROOT. mkbootimg needs the host's python3, and mkfs.ext4 -d builds the
# image from a directory without a loop mount — neither wants the emulator.
export KDOS_TARGET=aarch64-kdos-linux-musl
export KDOS_BOARD=${KDOS_BOARD:-fajita}

export WORKSPACE=/workspace
export BUILD_DIR=$WORKSPACE/build-mobile
export SYSROOT=$BUILD_DIR/fs
export IMAGES=$BUILD_DIR/images
export MARK=$BUILD_DIR/mark/packaging

mkdir -p $MARK $IMAGES

export SOURCE_DATE_EPOCH=1735689600
export TZ=UTC
export LC_ALL=C

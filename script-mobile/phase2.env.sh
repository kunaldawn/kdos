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
export KDOS_PHASE_TITLE="Self-Hosting Bootstrap"
export KDOS_PHASE_DESC="rebuild tar, musl, zlib, binutils and gcc inside the aarch64 chroot"
export KDOS_SNAPSHOT_PATHS="fs"
export KDOS_SNAPSHOT_EXCLUDE="fs/tmp/* fs/var/cache/kpkg/work/* fs/dev/* fs/proc/* fs/sys/* fs/run/* fs/kdos/* fs/ports/*"

# THE FLAG THAT SENDS THIS PHASE THROUGH chroot_exec.sh. The rootfs is
# aarch64 and the host is not, so every command here runs under the qemu-user
# binfmt handler and every ports/core recipe runs native-shaped and unmodified.
export CHROOT=1

# ports/mobile FIRST. PORT_REPO is an ordered search path and the first repo
# wins on a duplicate name, which is what makes ports/mobile an overlay rather
# than a fork: every core recipe is available, and a board-specific one shadows
# it by name alone. The paths are the chroot's, not the host's.
export PORT_REPO="/kdos/ports/mobile /ports/core"

export CFLAGS="-O2 -pipe"
export CXXFLAGS="-O2 -pipe"
export LDFLAGS=""
export SOURCE_DATE_EPOCH=1735689600
export TZ=UTC
export LC_ALL=C
export MAKEFLAGS="-j12"

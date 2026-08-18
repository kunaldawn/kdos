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

# ── reproducible packages (P12) ───────────────────────────────────────────
#
# A package built twice from this tree must be byte-identical. kpkg normalises
# the ARCHIVE itself (sorted, uid/gid 0, --mtime from SOURCE_DATE_EPOCH, pinned
# xz); these are the five environment lines that normalise what goes INTO it.
#
# The epoch is pinned rather than taken from the clock or from git — the build
# container has no git, and "now" is the single largest source of drift. TZ and
# LC_ALL because a few configure scripts and doc generators embed a formatted
# date or sort a list by locale. -ffile-prefix-map rewrites the build directory
# out of __FILE__ and DWARF paths, and --build-id=sha1 makes the note a function
# of the contents instead of a random 128-bit value.
export SOURCE_DATE_EPOCH=1735689600
export TZ=UTC
export LC_ALL=C
export CFLAGS="$CFLAGS -ffile-prefix-map=/var/cache/kpkg/work=/build"
export CXXFLAGS="$CXXFLAGS -ffile-prefix-map=/var/cache/kpkg/work=/build"
export LDFLAGS="$LDFLAGS -Wl,--build-id=sha1"
export MAKEFLAGS="-j12"

# ── strict recipe matching ────────────────────────────────────────────────
#
# kpkg's skip-if-installed compares the RECIPE, not merely the presence of a
# database entry: a port whose kpkgbuild, build.sh, postinstall.sh or patches
# have changed is rebuilt without having to be named on the command line.
# Without it, an edited recipe on an incremental tree installs nothing and the
# build reports success.
#
# The comparison is kp_recipe_hash(), the same SHA-256 the binhost's `E:` uses,
# so there is one definition of "the recipe changed". A package with no
# recorded hash reads as UNKNOWN and is left alone, so a tree that predates the
# record is not rebuilt wholesale.
#
# Set for the build; an interactive `kpkg install` is unaffected. It does NOT
# cover a file under fs/ that a port installs — that is not part of the recipe
# hash, and `release = N` is still the lever for those.
export KPKG_STRICT_RECIPE=1

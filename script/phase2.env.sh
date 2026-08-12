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
export KDOS_PHASE_TITLE="Self-Hosting Bootstrap"
export KDOS_PHASE_DESC="rebuild tar musl zlib binutils gcc inside the chroot"
export KDOS_SNAPSHOT_PATHS="fs"
export KDOS_SNAPSHOT_EXCLUDE="fs/tmp/* fs/var/cache/kpkg/work/* fs/dev/* fs/proc/* fs/sys/* fs/run/* fs/kdos/* fs/ports/*"

export CHROOT=1

export PKG_CONFIG_PATH="/usr/local/share/pkgconfig:/usr/local/lib/pkgconfig:/usr/local/lib64/pkgconfig:/usr/share/pkgconfig:/usr/lib/pkgconfig:/usr/lib64/pkgconfig"${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}
export CFLAGS="-O2 -pipe -std=gnu11 -fPIC"
export CXXFLAGS="-O2 -pipe -fPIC"
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
export TERM=dumb

rm -rf /var/cache/kpkg/work

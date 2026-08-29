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

# Environment configuration for the KDOS desktop phase.

# --- build-system metadata (PARSED by the orchestrator, never sourced) ---
export KDOS_PHASE_TITLE="Desktop"
export KDOS_PHASE_DESC="wlroots, kdos-comp, kdos-shell"
export KDOS_SNAPSHOT_PATHS="fs"
export KDOS_SNAPSHOT_EXCLUDE="fs/tmp/* fs/var/cache/kpkg/work/* fs/dev/* fs/proc/* fs/sys/* fs/run/* fs/kdos/* fs/ports/*"

export CHROOT=1
# src/desktop joins the port repos here rather than in every phase: it holds
# the compositor and the shell, which no earlier phase can build anyway.
export PORT_REPO="/ports/core /kdos/src/packages /kdos/src/desktop"

export PKG_CONFIG_PATH="/usr/local/share/pkgconfig:/usr/local/lib/pkgconfig:/usr/local/lib64/pkgconfig:/usr/share/pkgconfig:/usr/lib/pkgconfig:/usr/lib64/pkgconfig"${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}
# THE COMPILER IS NAMED, NOT DISCOVERED. autoconf's AC_PROG_CC walks a
# preference list, and some of them put clang FIRST — potrace's does. clang is
# a port now, so the moment it is installed those recipes silently change
# toolchain, and the failure is `C compiler cannot create executables` from a
# configure that had a working gcc in $PATH the whole time. A distro that
# builds itself cannot have its toolchain depend on which ports are installed.
export CC=gcc
export CXX=g++

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

rm -rf /var/cache/kpkg/work

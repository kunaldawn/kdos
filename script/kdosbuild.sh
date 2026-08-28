#!/bin/bash
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   script/kdosbuild.sh — compile the orchestrator, then run it
#
# `make build` comes through here. kdosbuild is built from source at run start
# rather than shipped as a binary or baked into the Dockerfile: it is a
# two-second compile of a program that links nothing but libc, and it keeps
# the image independent of the source tree it builds.

set -e
cd "$(dirname "$0")/.."

OUT=${KDOSBUILD_BIN:-build/.kdosbuild}
mkdir -p "$(dirname "$OUT")"

${CC:-cc} -O2 -std=gnu11 -D_GNU_SOURCE -Wall -Wextra \
    -Isrc/libs/libkbase -Isrc/libs/libkbuild -Isrc/libs/libktui \
    -Isrc/libs/libkcolor -Isrc/build/kdosbuild \
    -o "$OUT" \
    src/build/kdosbuild/*.c \
    src/libs/libkbase/*.c src/libs/libkbuild/*.c \
    src/libs/libktui/*.c src/libs/libkcolor/*.c

# THE BUILD RUNS AS THE CONTAINER'S ROOT and everything it writes under build/
# would stay root's — snapshots the developer cannot delete, logs they cannot
# read with their own editor, and an ISO `make run` cannot open. HOST_UID and
# HOST_GID come in from the Makefile for this; a trap rather than a trailing
# line, so a build that fails hands its logs back too.
# build/podman is the pack bake's own container store and is root's by design
# — podman refuses a store whose ownership does not match the user running it.
#
# AND `build/fs` IS THE TARGET ROOTFS, WHOSE OWNERSHIP IS THE SHIPPED SYSTEM'S.
# Handing it back does not make a developer's life easier, it corrupts the
# distribution: `chown` CLEARS THE SETUID BIT, so every privileged binary in
# the image loses it — `kdos-checkpass`, whose loss refuses every password and
# locks the user out of their own session; `kdos-resctl`; and
# `newuidmap`/`newgidmap`, without which no rootless container can be created
# and no box on the machine starts. It also rewrites every file's OWNER, so a
# tree that `make install` correctly left to root comes out owned by uid 1000 —
# `/etc/shadow` and `/etc/sudoers` included, on a system where uid 1000 is the
# desktop user.
#
# It bites on the SECOND build and every one after: the squashfs is made inside
# the chroot before this trap runs, so a single-pass build ships correct bits
# and an incremental one squashes what the previous build's exit stripped.
# Reading `build/fs` from the host now needs a container or sudo, which is the
# correct price for a rootfs.
hand_back() {
    [ -n "${HOST_UID:-}" ] || return 0
    find build -mindepth 1 -maxdepth 1 ! -name podman ! -name fs \
        -exec chown -R "$HOST_UID:${HOST_GID:-$HOST_UID}" {} + 2>/dev/null || true
}
trap hand_back EXIT

"$OUT" --script-dir script "$@"

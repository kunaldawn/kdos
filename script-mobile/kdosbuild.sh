#!/bin/bash

# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   script-mobile/kdosbuild.sh — compile the orchestrator, then run it for aarch64
#
# `make build-mobile` comes through here. The orchestrator itself is arch-
# neutral: --script-dir and --build-dir are all that separate the two targets,
# so this is script/kdosbuild.sh with those two flags pointed at the mobile
# tree. It is compiled from source at run start for the same reason the desktop
# wrapper is — a two-second compile keeps the image independent of the tree.

set -e
cd "$(dirname "$0")/.."

OUT=${KDOSBUILD_BIN:-build-mobile/.kdosbuild}
mkdir -p "$(dirname "$OUT")"

${CC:-cc} -O2 -std=gnu11 -D_GNU_SOURCE -Wall -Wextra \
    -Isrc/libs/libkbase -Isrc/libs/libkbuild -Isrc/libs/libktui \
    -Isrc/libs/libkcolor -Isrc/build/kdosbuild \
    -o "$OUT" \
    src/build/kdosbuild/*.c \
    src/libs/libkbase/*.c src/libs/libkbuild/*.c \
    src/libs/libktui/*.c src/libs/libkcolor/*.c

# Hand the build tree back to the caller, exactly as the desktop wrapper does
# and for the same reasons — see script/kdosbuild.sh, which states them.
# build-mobile/fs is excluded there too: it is the target rootfs, and chown
# clears the setuid bit on every privileged binary in the shipped image.
hand_back() {
    [ -n "${HOST_UID:-}" ] || return 0
    find build-mobile -mindepth 1 -maxdepth 1 ! -name fs \
        -exec chown -R "$HOST_UID:${HOST_GID:-$HOST_UID}" {} + 2>/dev/null || true
}
trap hand_back EXIT

"$OUT" --script-dir script-mobile --build-dir build-mobile "$@"

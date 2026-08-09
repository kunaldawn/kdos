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

exec "$OUT" --script-dir script "$@"

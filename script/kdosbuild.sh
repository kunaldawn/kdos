#!/bin/bash
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   script/kdosbuild.sh — compile the orchestrator when it changed, then run it
#
# `make build` comes through here. kdosbuild is built from source rather than
# shipped as a binary or baked into the Dockerfile, which keeps the image
# independent of the source tree it builds. It is compiled only when its
# sources, its flags or the compiler differ from what built the binary beside
# it: a hash of all three sits in "$OUT.sum", written only after a compile
# succeeds, so a failed compile or a binary built by another machine's
# compiler (the host's glibc one against the image's musl one) is never run.

set -e
cd "$(dirname "$0")/.."

OUT=${KDOSBUILD_BIN:-build/.kdosbuild}
mkdir -p "$(dirname "$OUT")"

# libkpkg is on the list twice over: libkbuild walks the ports tree through it
# (the picker's port list comes from the same walker kpkg resolves names with),
# and the orchestrator asks kpkg's kp_installed_current() on the host to decide
# which ports of a chroot package phase become steps.
LIBS=(libkbase libkbuild libkpkg libktui libkcolor)
FLAGS=(-O2 -std=gnu11 -D_GNU_SOURCE -Wall -Wextra)
SRCS=(src/devtools/kdosbuild)
for l in "${LIBS[@]}"; do
    FLAGS+=("-Isrc/libs/$l")
    SRCS+=("src/libs/$l")
done
FLAGS+=(-Isrc/devtools/kdosbuild)

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
# Reading `build/fs` from the host needs a container or sudo, which is the
# correct price for a rootfs.
hand_back() {
    [ -n "${HOST_UID:-}" ] || return 0
    find build -mindepth 1 -maxdepth 1 ! -name podman ! -name fs \
        -exec chown -R "$HOST_UID:${HOST_GID:-$HOST_UID}" {} + 2>/dev/null || true
    chown "$HOST_UID:${HOST_GID:-$HOST_UID}" "$OUT" "$OUT.sum" 2>/dev/null || true
}
trap hand_back EXIT

CCBIN=${CC:-cc}
SUM=$({
    printf '%s\n' "$CCBIN" "${FLAGS[@]}"
    $CCBIN -dumpmachine
    $CCBIN --version | head -n 1
    find "${SRCS[@]}" -type f -name '*.[ch]' -print0 | LC_ALL=C sort -z \
        | xargs -0 sha256sum
} | sha256sum)
SUM=${SUM%% *}

if [ ! -x "$OUT" ] || [ "$(cat "$OUT.sum" 2>/dev/null)" != "$SUM" ]; then
    rm -f "$OUT.sum"
    CFILES=()
    for d in "${SRCS[@]}"; do
        CFILES+=("$d"/*.c)
    done
    $CCBIN "${FLAGS[@]}" -o "$OUT" "${CFILES[@]}"
    printf '%s\n' "$SUM" > "$OUT.sum"
fi

"$OUT" --script-dir script "$@"

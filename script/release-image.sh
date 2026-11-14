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
#
# script/release-image.sh — a release's signed whole-slot image, and its
# deltas from the releases before it.
#
#   script/release-image.sh --root DIR [--upper DIR] --out DIR --key FILE
#                           [--version V] [--prev DIR [--prev-root DIR]]...
#                           [--level N]
#
#   --root       the root the ISO's squashfs is made from, exactly: what the
#                image holds is what a fresh install holds
#   --upper      the image stage's upper layer, build/image-upper: the root
#                is then that layer over --root (build/fs), mounted
#                read-only in this run's own mount namespace, which is the
#                tree a KDOS_IMAGE=base build squashed. Without it a base
#                build's image would carry every package build/fs holds
#   --out        where the assets go, every name flat: kdos-image.manifest,
#                its .sig, the full bundle's parts kdos-image-<V>.full.NNN,
#                one kdos-image-<V>.from-<P>.delta per --prev, and the
#                release index kdos-update.idx and its .sig, which name each
#                of them at image/<name> with its size and SHA-256 and are
#                what `kdos image unpack` and `kdos update` read first;
#                `ports/publish --release --image <dir>` publishes them as
#                the release image-<version>
#   --key        the release key (kdos-release.key); its .pub beside it is
#                what /etc/kdos/keys holds on every machine that takes it
#   --version    the release, by default the `version` line of
#                <root>/ports/TREE
#   --prev       an earlier release's image assets — the directory holding
#                its kdos-image.manifest — once for each release a machine
#                may still run; two covers the inactive slot's n-1 and n-2
#   --prev-root  that release's root, unpacked: with it the delta carries a
#                changed file as a zstd patch against the old one, without
#                it every changed file whole
#
# THE FORMAT CODE IS THIS CHECKOUT'S. kdos-tools is compiled here from
# src/system/kdos-tools, the code every machine runs to read the image, and
# never taken from the host or the root being imaged. The image is verified
# against the key's public half before the run ends.
#
# Run it as root: the root holds files only root can read, and an image of
# the readable part would be a root that boots without them.

set -euo pipefail

usage() {
    sed -n '16,44p' "$0" | sed 's/^# \{0,1\}//'
    exit 2
}

ORIG=("$@")
ROOT="" UPPER="" OUT="" KEY="" VERSION="" LEVEL=12
PREV=() PREVROOT=()
while [ $# -gt 0 ]; do
    case $1 in
        --root)      ROOT=${2:?}; shift 2 ;;
        --upper)     UPPER=${2:?}; shift 2 ;;
        --out)       OUT=${2:?}; shift 2 ;;
        --key)       KEY=${2:?}; shift 2 ;;
        --version)   VERSION=${2:?}; shift 2 ;;
        --level)     LEVEL=${2:?}; shift 2 ;;
        --prev)      PREV+=("${2:?}"); PREVROOT+=(""); shift 2 ;;
        --prev-root) [ ${#PREV[@]} -gt 0 ] || usage
                     PREVROOT[${#PREV[@]}-1]=${2:?}; shift 2 ;;
        *)           usage ;;
    esac
done
[ -n "$ROOT" ] && [ -n "$OUT" ] && [ -n "$KEY" ] || usage
[ -d "$ROOT" ] || { echo "release-image: $ROOT is not a directory" >&2; exit 2; }
[ -z "$UPPER" ] || [ -d "$UPPER" ] ||
    { echo "release-image: $UPPER is not a directory" >&2; exit 2; }

# THE STAGE IS MOUNTED IN A NAMESPACE OF THIS RUN'S OWN, as script/chroot/exec.sh
# mounts it for each 70_image step: nothing outside sees it, and it goes with
# the run. KDOS_RELIMG_NS marks the re-executed copy.
if [ -n "$UPPER" ] && [ -z "${KDOS_RELIMG_NS:-}" ]; then
    KDOS_RELIMG_NS=1 exec unshare --mount --propagation private -- bash "$0" "${ORIG[@]}"
fi
[ -f "$KEY" ] && [ -f "${KEY%.key}.pub" ] ||
    { echo "release-image: $KEY and ${KEY%.key}.pub are both needed" >&2; exit 2; }

HERE=$(cd "$(dirname "$0")/.." && pwd)
TOOLS=$(mktemp -d "${TMPDIR:-/tmp}/kdos-release-image.XXXXXX")
trap 'umount "$TOOLS/stage" 2>/dev/null; rm -rf "$TOOLS"' EXIT
if [ -n "$UPPER" ]; then
    mkdir "$TOOLS/stage"
    mount -t overlay overlay -o "ro,lowerdir=$(readlink -f "$UPPER"):$(readlink -f "$ROOT")" "$TOOLS/stage"
    ROOT=$TOOLS/stage
fi

L="$HERE/src/libs"
${CC:-cc} -O2 -std=gnu11 -D_GNU_SOURCE -Wall -Wextra \
    -I"$L/libkbase" -I"$L/libkcolor" -I"$L/libkpkg" -I"$L/libkxdg" \
    -I"$L/libkproc" -I"$L/libksig" -I"$L/libkpack" -I"$L/libkbuild" \
    -I"$HERE/src/system/kdos-tools" -o "$TOOLS/kdos-tools" \
    "$HERE"/src/system/kdos-tools/*.c "$L"/libkbase/*.c "$L"/libkcolor/*.c \
    "$L"/libkpkg/*.c "$L"/libkxdg/*.c "$L"/libkproc/*.c "$L"/libksig/*.c \
    "$L"/libksig/monocypher/*.c "$L"/libkpack/*.c "$L"/libkbuild/kb_json.c -ldl
ln -s kdos-tools "$TOOLS/kdos"

args=(--key "$KEY" --level "$LEVEL")
[ -n "$VERSION" ] && args+=(--version "$VERSION")
for i in "${!PREV[@]}"; do
    args+=(--prev "${PREV[$i]}")
    [ -n "${PREVROOT[$i]}" ] && args+=(--prev-root "${PREVROOT[$i]}")
done
mkdir -p "$OUT"
"$TOOLS/kdos" image make "$ROOT" "$OUT" "${args[@]}"

mkdir "$TOOLS/keys"
cp "${KEY%.key}.pub" "$TOOLS/keys/"
KPKG_KEYRING="$TOOLS/keys" "$TOOLS/kdos" image verify "$OUT"

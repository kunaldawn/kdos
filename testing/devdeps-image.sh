#!/usr/bin/env bash
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   testing/devdeps-image.sh — build kdos-devdeps:latest, then run a suite in it
#
#   testing/devdeps-image.sh                     testing/selftest.sh
#   testing/devdeps-image.sh plasmatest [...]    testing/plasmatest.sh [...]
#   testing/devdeps-image.sh -- <command...>     anything else
#
# With no argument it builds the image and runs testing/selftest.sh inside it,
# which is the run where nothing is skipped. `plasmatest` runs the Qt suite,
# which runs nowhere else, and hands it the rest of the line. Anything after
# -- is run as it is. The checkout is mounted at /kdos.
#
# The image builds the tree's own Discover release, because kdos-discover is
# version-locked to it; the tree's own plasma-welcome, by its recipe's
# build.sh and patches, because the KDOS Welcome pages call what those patches
# add; and carries the licence texts of the tree's plasma-desktop, which the
# SDDM theme's build copies. The version and sha256 of each are read out of
# its recipe here, and plasma-welcome's recipe directory is the build context
# `welcome`, so a Plasma bump or a changed patch rebuilds those layers and
# nothing else does.
set -euo pipefail
cd "$(dirname "$0")/.."

# The version and sha256 of the tree's recipe for one port, as the build
# arguments <PREFIX>_VERSION and <PREFIX>_SHA256.
recipe_args() {   # <port> <PREFIX>
    local recipe dv ds
    recipe=$(find ports/core -mindepth 3 -maxdepth 3 -path "*/$1/kpkgbuild" 2>/dev/null | head -1)
    [ -n "$recipe" ] || return 0
    dv=$(sed -n 's/^version[[:blank:]]*=[[:blank:]]*//p' "$recipe" | head -1)
    ds=$(sed -n 's/^sha256[[:blank:]]*=[[:blank:]]*\([0-9a-f]\{64\}\)[[:blank:]].*/\1/p' "$recipe" | head -1)
    [ -n "$dv" ] && [ -n "$ds" ] || { echo "devdeps-image: $recipe names no version or sha256" >&2; exit 1; }
    args+=(--build-arg "$2_VERSION=$dv" --build-arg "$2_SHA256=$ds")
}

args=()
recipe_args discover DISCOVER
recipe_args plasma-welcome WELCOME
recipe_args plasma-desktop DESKTOP
welcome=$(find ports/core -mindepth 2 -maxdepth 2 -type d -name plasma-welcome | head -1)
[ -n "$welcome" ] || { echo "devdeps-image: the tree has no plasma-welcome recipe" >&2; exit 1; }
args+=(--build-context "welcome=$welcome")

docker build "${args[@]}" -f testing/Dockerfile.devdeps -t kdos-devdeps:latest testing/

case "${1:-}" in
    "")         set -- bash testing/selftest.sh ;;
    plasmatest) shift; set -- bash testing/plasmatest.sh "$@" ;;
    --)         shift ;;
esac
exec docker run --rm -v "$PWD":/kdos -w /kdos kdos-devdeps:latest "$@"

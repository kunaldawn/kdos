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
# version-locked to it; its version and sha256 are read out of the discover
# recipe here, so a Plasma bump rebuilds that layer and nothing else does.
set -euo pipefail
cd "$(dirname "$0")/.."

args=()
recipe=$(find ports/core -mindepth 3 -maxdepth 3 -path '*/discover/kpkgbuild' 2>/dev/null | head -1)
if [ -n "$recipe" ]; then
    dv=$(sed -n 's/^version[[:blank:]]*=[[:blank:]]*//p' "$recipe" | head -1)
    ds=$(sed -n 's/^sha256[[:blank:]]*=[[:blank:]]*\([0-9a-f]\{64\}\)[[:blank:]].*/\1/p' "$recipe" | head -1)
    [ -n "$dv" ] && [ -n "$ds" ] || { echo "devdeps-image: $recipe names no version or sha256" >&2; exit 1; }
    args=(--build-arg "DISCOVER_VERSION=$dv" --build-arg "DISCOVER_SHA256=$ds")
fi

docker build "${args[@]}" -f testing/Dockerfile.devdeps -t kdos-devdeps:latest testing/

case "${1:-}" in
    "")         set -- bash testing/selftest.sh ;;
    plasmatest) shift; set -- bash testing/plasmatest.sh "$@" ;;
    --)         shift ;;
esac
exec docker run --rm -v "$PWD":/kdos -w /kdos kdos-devdeps:latest "$@"

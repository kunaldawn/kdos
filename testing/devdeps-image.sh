#!/usr/bin/env bash
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   testing/devdeps-image.sh — build kdos-devdeps:latest, then run the suite
#
# With no argument it builds the image and runs testing/selftest.sh inside it,
# which is the run where nothing is skipped. Anything after -- is run instead.
set -euo pipefail
cd "$(dirname "$0")/.."

docker build -f testing/Dockerfile.devdeps -t kdos-devdeps:latest testing/

if [ "$#" -eq 0 ]; then
    set -- bash testing/selftest.sh
fi
exec docker run --rm -v "$PWD":/kdos -w /kdos kdos-devdeps:latest "$@"

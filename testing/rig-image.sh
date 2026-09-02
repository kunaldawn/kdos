#!/usr/bin/env bash
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   testing/rig-image.sh — build the rig container
#
# The only step between a clone and a photograph of a booted KDOS. It needs the
# network once; everything after it runs offline against the ISO.
set -euo pipefail
cd "$(dirname "$0")/.."

exec docker build -f testing/Dockerfile.qemu -t kdos-qemu-py:latest testing/

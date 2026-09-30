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
# Hold the package store under KDOS_PKG_STORE_MAX: evict entries, least
# recently used first, until build/pkgstore fits. Runs inside the chroot, once
# every port of the build has been installed or reused, so every entry this
# build touched is newer than every one it did not. Nothing to do when the
# store is off.

set -e
source script/phases/70_image/phase.env

if [ -z "${KPKG_STORE:-}" ]; then
    echo "[PKGSTORE] The package store is off (KDOS_PKG_STORE=${KDOS_PKG_STORE:-0})."
    exit 0
fi

kpkg store gc "$KPKG_STORE" "${KDOS_PKG_STORE_MAX:-60G}"

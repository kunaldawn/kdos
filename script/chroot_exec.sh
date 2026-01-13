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

set -e

# Detect repository root
SCRIPT_DIR="$(dirname "$(readlink -f "$0")")"
REPO_ROOT="$(dirname "$SCRIPT_DIR")"
CHROOT_DIR="$REPO_ROOT/build/fs"

if [ "$(id -u)" -ne 0 ]; then
    echo "Error: This script must be run as root (or with sudo) for chroot execution."
    exit 1
fi

if [ ! -d "$CHROOT_DIR" ]; then
    echo "Error: Chroot directory $CHROOT_DIR does not exist"
    exit 1
fi

# Ensure mount points exist
mkdir -p "$CHROOT_DIR/dev"
mkdir -p "$CHROOT_DIR/proc"
mkdir -p "$CHROOT_DIR/sys"
mkdir -p "$CHROOT_DIR/tmp"
mkdir -p "$CHROOT_DIR/run"
mkdir -p "$CHROOT_DIR/ports"
mkdir -p "$CHROOT_DIR/kdos"

# Mount virtual filesystems (Idempotent)
mountpoint -q "$CHROOT_DIR/dev" || mount --bind /dev "$CHROOT_DIR/dev"
mountpoint -q "$CHROOT_DIR/proc" || mount -t proc proc "$CHROOT_DIR/proc"
mountpoint -q "$CHROOT_DIR/sys" || mount -t sysfs sysfs "$CHROOT_DIR/sys"
mountpoint -q "$CHROOT_DIR/tmp" || mount -t tmpfs tmpfs "$CHROOT_DIR/tmp"
mountpoint -q "$CHROOT_DIR/run" || mount -t tmpfs tmpfs "$CHROOT_DIR/run"

# Mount repository and ports
mountpoint -q "$CHROOT_DIR/kdos" || mount --bind "$REPO_ROOT" "$CHROOT_DIR/kdos"
mkdir -p "$CHROOT_DIR/kdos/build"
mountpoint -q "$CHROOT_DIR/kdos/build" || mount --bind "$REPO_ROOT/build" "$CHROOT_DIR/kdos/build"
mountpoint -q "$CHROOT_DIR/ports" || mount --bind "$REPO_ROOT/ports" "$CHROOT_DIR/ports"

# Cleanup function (Unmount on exit)
cleanup() {
    umount "$CHROOT_DIR/ports" 2>/dev/null || true
    umount "$CHROOT_DIR/kdos/build" 2>/dev/null || true
    umount "$CHROOT_DIR/kdos" 2>/dev/null || true
    umount "$CHROOT_DIR/run" 2>/dev/null || true
    umount "$CHROOT_DIR/tmp" 2>/dev/null || true
    umount "$CHROOT_DIR/sys" 2>/dev/null || true
    umount "$CHROOT_DIR/proc" 2>/dev/null || true
    umount "$CHROOT_DIR/dev" 2>/dev/null || true
}

trap cleanup EXIT

# Execute command inside chroot
# We cd to /kdos to maintain relative path assumptions for scripts
# We use /usr/bin/env -i to clear host environment ensuring isolation
# But we keep PATH (basic) and TERM
chroot "$CHROOT_DIR" /usr/bin/env -i \
    HOME=/root \
    TERM="$TERM" \
    PATH=/bin:/usr/bin:/sbin:/usr/sbin \
    /bin/bash -c "cd /kdos && exec \"\$@\"" -- "$@"

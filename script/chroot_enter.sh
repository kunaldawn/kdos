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
    echo "Please run as root (sudo)"
    exit 1
fi

if [ ! -d "$CHROOT_DIR" ]; then
    echo "Error: Chroot directory $CHROOT_DIR does not exist"
    exit 1
fi

echo "Setting up chroot environment at $CHROOT_DIR..."

# Create mount points if they don't exist
mkdir -p "$CHROOT_DIR/dev"
mkdir -p "$CHROOT_DIR/proc"
mkdir -p "$CHROOT_DIR/sys"
mkdir -p "$CHROOT_DIR/tmp"
mkdir -p "$CHROOT_DIR/run"

# Unmount if already mounted to prevent errors
umount "$CHROOT_DIR/run" 2>/dev/null || true
umount "$CHROOT_DIR/tmp" 2>/dev/null || true
umount "$CHROOT_DIR/sys" 2>/dev/null || true
umount "$CHROOT_DIR/proc" 2>/dev/null || true
umount "$CHROOT_DIR/dev" 2>/dev/null || true

# Mount virtual filesystems
mount --bind /dev "$CHROOT_DIR/dev"
mount -t proc proc "$CHROOT_DIR/proc"
mount -t sysfs sysfs "$CHROOT_DIR/sys"
mount -t tmpfs tmpfs "$CHROOT_DIR/tmp"
mount -t tmpfs tmpfs "$CHROOT_DIR/run"

# Copy resolv.conf for networking
cp /etc/resolv.conf "$CHROOT_DIR/etc/resolv.conf" 2>/dev/null || true

# Cleanup function
cleanup() {
    echo "Unmounting filesystems..."
    umount "$CHROOT_DIR/run" 2>/dev/null || true
    umount "$CHROOT_DIR/tmp" 2>/dev/null || true
    umount "$CHROOT_DIR/sys" 2>/dev/null || true
    umount "$CHROOT_DIR/proc" 2>/dev/null || true
    umount "$CHROOT_DIR/dev" 2>/dev/null || true
}

# Trap exit to ensure cleanup
trap cleanup EXIT

echo "Entering chroot..."
if [ $# -gt 0 ]; then
    chroot "$CHROOT_DIR" "$@"
else
    chroot "$CHROOT_DIR" /usr/bin/bash -l
fi

echo "Exited chroot."

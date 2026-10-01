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

if [ "$EUID" -ne 0 ]; then
    echo "Please run as root (sudo)"
    exit 1
fi

# The shell runs in its own private mount namespace, as script/chroot/exec.sh
# does: its /proc, /dev, /sys, /tmp and /run are visible to nothing else, a
# build running beside it keeps its own, and all of them go away when the shell
# exits. KDOS_CHROOT_NS marks the re-executed copy.
if [ -z "${KDOS_CHROOT_NS:-}" ]; then
    KDOS_CHROOT_NS=1 exec unshare --mount --propagation private -- bash "$0" "$@"
fi
unset KDOS_CHROOT_NS

# The repository root is two levels above this file, script/chroot/.
SCRIPT_DIR="$(readlink -f "$0")"
SCRIPT_DIR="${SCRIPT_DIR%/*}"
REPO_ROOT="${SCRIPT_DIR%/*/*}"
CHROOT_DIR="$REPO_ROOT/build/fs"

if [ ! -d "$CHROOT_DIR" ]; then
    echo "Error: Chroot directory $CHROOT_DIR does not exist"
    exit 1
fi

echo "Setting up chroot environment at $CHROOT_DIR..."

mkdir -p "$CHROOT_DIR"/{dev,proc,sys,tmp,run}

# Mount virtual filesystems
mount --bind /dev "$CHROOT_DIR/dev"
mount -t proc proc "$CHROOT_DIR/proc"
mount -t sysfs sysfs "$CHROOT_DIR/sys"
mount -t tmpfs tmpfs "$CHROOT_DIR/tmp"
mount -t tmpfs tmpfs "$CHROOT_DIR/run"

# Copy resolv.conf for networking
cp /etc/resolv.conf "$CHROOT_DIR/etc/resolv.conf" 2>/dev/null || true

echo "Entering chroot..."
if [ $# -gt 0 ]; then
    chroot "$CHROOT_DIR" "$@"
else
    chroot "$CHROOT_DIR" /usr/bin/bash -l
fi

echo "Exited chroot."

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

# Diagnostics go to a file, never to stdout/stderr: the orchestrator parses
# of commands run through this wrapper (kpkgdepends prints the install order
# and nothing else), so a stray message here becomes a bogus package name.
MOUNT_LOG="$REPO_ROOT/build/logs/chroot.log"
log_mount() {
    mkdir -p "$(dirname "$MOUNT_LOG")" 2>/dev/null || return 0
    echo "$(date '+%Y-%m-%d %H:%M:%S') $*" >> "$MOUNT_LOG" 2>/dev/null || true
}

# List every mountpoint at or below the chroot, deepest first.
list_mounts() {
    cut -d' ' -f2 /proc/self/mounts \
        | awk -v dir="$CHROOT_DIR" '$0 == dir || index($0, dir "/") == 1' \
        | sort -r
}

# Unmount everything under the chroot. A plain `umount ... || true` is not
# enough: one transient EBUSY leaves a bind mount behind, the next run skips
# re-mounting it because mountpoint(1) still says yes, and the phase snapshot
# then refuses to archive a tree with live mounts in it.
unmount_all() {
    local mp attempt left
    for attempt in 1 2 3; do
        left=0
        for mp in $(list_mounts); do
            umount "$mp" 2>/dev/null && continue
            sleep 0.2
            umount "$mp" 2>/dev/null || left=1
        done
        [ "$left" -eq 0 ] && return 0
    done

    # Still stuck: detach lazily so the tree is at least clean for the next run.
    for mp in $(list_mounts); do
        log_mount "warning: lazily detaching busy mount $mp"
        umount -l "$mp" 2>/dev/null || true
    done

    for mp in $(list_mounts); do
        log_mount "ERROR: could not unmount $mp"
    done
}

# Clear anything a previously killed run left behind, then mount fresh.
unmount_all

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

# Explicitly mount sub-mounts that might be hidden by the main bind mount
mkdir -p "$CHROOT_DIR/kdos/script"
mountpoint -q "$CHROOT_DIR/kdos/script" || mount --bind "$REPO_ROOT/script" "$CHROOT_DIR/kdos/script"
mkdir -p "$CHROOT_DIR/kdos/src"
mountpoint -q "$CHROOT_DIR/kdos/src" || mount --bind "$REPO_ROOT/src" "$CHROOT_DIR/kdos/src"

cleanup() {
    unmount_all
}

trap cleanup EXIT

# Execute command inside chroot
# We cd to /kdos to maintain relative path assumptions for scripts
# We use /usr/bin/env -i to clear host environment ensuring isolation
# But we keep PATH (basic) and TERM
# KDOS_REPLAY is forwarded: it tells a step that the developer picked it
# deliberately, so mark-file guards ("already built, exit 0") stand down.
#
# SO ARE THE TWO OPT-IN PACKAGING KNOBS, and they have to be named here for the
# same reason: `env -i` clears the environment, so a variable the Makefile
# passes into the container reaches every step that runs on the HOST and none
# that runs in the chroot. 06_packaging is a chroot phase, which is where both
# of these are read — a `make build KDOS_ISO_SOURCES=1` that arrives here
# unnamed produces an ordinary stick and says nothing about why.
#
# /usr/local/bin is LAST, unlike fs/etc/profile which puts it first. Our own
# tools install there — kdos, kdos-appbox — and leaving it out entirely made
# `kdos-appbox image assemble` in 01_appbox.sh die with "command not found",
# which podman then reported as an unreadable image format. Appending fixes
# that without letting a /usr/local/bin binary shadow a /usr/bin one during a
# port's configure, which is a different bug and a much harder one to see.
chroot "$CHROOT_DIR" /usr/bin/env -i \
    HOME=/root \
    TERM="$TERM" \
    KDOS_REPLAY="${KDOS_REPLAY:-0}" \
    KDOS_ISO_SOURCES="${KDOS_ISO_SOURCES:-0}" \
    KDOS_PACK_KDOS="${KDOS_PACK_KDOS:-0}" \
    PATH=/bin:/usr/bin:/sbin:/usr/sbin:/usr/local/bin \
    /bin/bash -c "cd /kdos && exec \"\$@\"" -- "$@"

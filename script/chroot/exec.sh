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
    echo "Error: This script must be run as root (or with sudo) for chroot execution."
    exit 1
fi

# Every entry runs in its own private mount namespace. Each has its own /proc,
# /dev, /sys, /tmp, /run and repository binds: nothing it mounts is visible to
# a concurrent entry or to the container, and all of it disappears with the
# entry's last process, so there is nothing to unmount and nothing a killed
# run can leave behind. Concurrent entries are safe for that reason alone.
# KDOS_CHROOT_NS marks the re-executed copy; it is not forwarded into the
# chroot.
if [ -z "${KDOS_CHROOT_NS:-}" ]; then
    KDOS_CHROOT_NS=1 exec unshare --mount --propagation private -- bash "$0" "$@"
fi

# The repository root is two levels above this file, script/chroot/.
SCRIPT_DIR="$(readlink -f "$0")"
SCRIPT_DIR="${SCRIPT_DIR%/*}"
REPO_ROOT="${SCRIPT_DIR%/*/*}"
CHROOT_DIR="$REPO_ROOT/build/fs"

if [ ! -d "$CHROOT_DIR" ]; then
    echo "Error: Chroot directory $CHROOT_DIR does not exist"
    exit 1
fi

# Diagnostics go to a file, never to stdout/stderr: the orchestrator parses the output
# of commands run through this wrapper (kpkgdepends prints the install order
# and nothing else), so a stray message here becomes a bogus package name.
MOUNT_LOG="$REPO_ROOT/build/logs/chroot.log"
log_mount() {
    mkdir -p "${MOUNT_LOG%/*}" 2>/dev/null || return 0
    printf '%(%Y-%m-%d %H:%M:%S)T %s\n' -1 "$*" >> "$MOUNT_LOG" 2>/dev/null || true
}

mkdir -p "$CHROOT_DIR"/{dev,proc,sys,tmp,run,ports,kdos}

# Virtual filesystems. The namespace is fresh, so every mount is made
# unconditionally: none of them can already be there.
mount --bind /dev "$CHROOT_DIR/dev"
mount -t proc proc "$CHROOT_DIR/proc"
mount -t sysfs sysfs "$CHROOT_DIR/sys"
# The cgroup tree, read-only, over the fresh sysfs's empty /sys/fs/cgroup.
# ninja, cargo and go size themselves from cpu.max through it; without it they
# see every host thread whatever --cpus cap the container runs under. The bind
# and the remount are two steps because busybox mount ignores ro on a bind.
if [ -d /sys/fs/cgroup ]; then
    if mount --bind /sys/fs/cgroup "$CHROOT_DIR/sys/fs/cgroup"; then
        mount -o remount,bind,ro "$CHROOT_DIR/sys/fs/cgroup" \
            || { umount "$CHROOT_DIR/sys/fs/cgroup"; log_mount "warning: cgroup tree not mounted: no read-only bind"; }
    else
        log_mount "warning: cgroup tree not mounted"
    fi
fi
mount -t tmpfs tmpfs "$CHROOT_DIR/tmp"
mount -t tmpfs tmpfs "$CHROOT_DIR/run"

# Mount repository and ports
mount --bind "$REPO_ROOT" "$CHROOT_DIR/kdos"
mkdir -p "$CHROOT_DIR"/kdos/{build,script,src,fs}
mount --bind "$REPO_ROOT/build" "$CHROOT_DIR/kdos/build"
mount --bind "$REPO_ROOT/ports" "$CHROOT_DIR/ports"

# Explicitly mount sub-mounts that might be hidden by the main bind mount
mount --bind "$REPO_ROOT/script" "$CHROOT_DIR/kdos/script"
mount --bind "$REPO_ROOT/src" "$CHROOT_DIR/kdos/src"
# fs/ too: a medium that carries the sources copies the overlay from here, and
# without the bind /kdos/fs is the container's empty mount point.
mount --bind "$REPO_ROOT/fs" "$CHROOT_DIR/kdos/fs"

# AT LEAST 4096 OPEN FILES, soft and hard. QtWebEngine's configure writes a
# linker wrapper that runs `ulimit -n 4096` before its bfd link, and that call
# fails, and the link with it, when the hard limit is lower. Root may raise
# the hard limit; a limit already at or above 4096 is left alone.
_nofile=$(ulimit -Hn)
if [ "$_nofile" != unlimited ] && [ "$_nofile" -lt 4096 ]; then
    ulimit -Hn 4096 2>/dev/null || log_mount "warning: hard open-files limit stays at $_nofile"
fi
_nofile=$(ulimit -Sn)
if [ "$_nofile" != unlimited ] && [ "$_nofile" -lt 4096 ]; then
    ulimit -Sn 4096 2>/dev/null || log_mount "warning: soft open-files limit stays at $_nofile"
fi

case "${KDOS_PKG_STORE:-0}" in
    1)     _store=/kdos/build/pkgstore; _store_check=0 ;;
    check) _store=/kdos/build/pkgstore; _store_check=1 ;;
    *)     _store=; _store_check=0 ;;
esac

# Execute command inside chroot, from /kdos so relative paths hold, with an
# environment cleared by `env -i` down to HOME, TERM, PATH and the variables
# named on that line.
#
# EVERY BUILD KNOB HAS TO BE NAMED ON THAT LINE: `env -i` clears the
# environment, so a variable the Makefile passes into the container reaches
# every step that runs on the HOST and none that runs in the chroot — a
# `make build KDOS_ISO_SOURCES=1` that arrives here unnamed produces an
# ordinary stick and says nothing about why. What each forwarded one is:
#   KDOS_REPLAY        the developer picked this step deliberately, so
#                      mark-file guards ("already built, exit 0") stand down
#   KDOS_JOBS          the job count; empty lets common.env compute it inside
#                      the chroot the way it does on the host
#   KDOS_ISO_SOURCES,  opt-in packaging, read by 70_image
#   KDOS_PACK_KDOS,
#   KDOS_MAKE_BINHOST
#   KDOS_ISO_COMP      system.sfs codec, read by 70_image/110_iso.sh; empty
#                      is the release default there
#   KDOS_CCACHE        1 (the default) caches CMake compiles in build/ccache;
#                      0 turns the cache off, read by script/env/chroot.env
#   KPKG_KEEP_CACHE    KDOS_MAKE_BINHOST again, so every chroot `kpkg install`
#                      keeps the package it built for 70_image to index
#   KDOS_PKG_STORE,    the package store: 0 (the default) off, 1 on, check
#   KDOS_PKG_STORE_MAX builds every hit and compares; the cap 70_image's
#                      015_pkgstore.sh evicts down to
#   KPKG_STORE,        what kpkg reads: the store directory, empty when off,
#   KPKG_STORE_CHECK   and 1 for check mode
#   KPKG_STORE_SALT    set by kdosbuild on the host: the hash of what the
#                      bootstrap phases built the chroot from
#
# /usr/local/bin is LAST, unlike fs/etc/profile which puts it first. Our own
# tools install there — kdos, kdos-appbox — and 70_image calls kdos-appbox
# by name: leave it off and `kdos-appbox genlaunchers` in 030_launchers.sh is not
# on $PATH, that step's own guard reports it as not installed and exits 0, and
# the image ships launchers for packs it does not carry. Appending fixes that
# without letting a /usr/local/bin binary shadow a /usr/bin one during a
# port's configure, which is a different bug and a much harder one to see.
#
# /usr/bin comes before /bin. /bin is a link to usr/bin, but CMake turns each
# PATH entry into a search prefix: with /bin first it finds a package's CMake
# config under /lib/cmake, and a config that computes its prefix from its own
# location (harfbuzz, Qt) then answers / and points at //include and //share.
exec chroot "$CHROOT_DIR" /usr/bin/env -i \
    HOME=/root \
    TERM="$TERM" \
    KDOS_REPLAY="${KDOS_REPLAY:-0}" \
    KDOS_JOBS="${KDOS_JOBS:-}" \
    KDOS_ISO_SOURCES="${KDOS_ISO_SOURCES:-0}" \
    KDOS_PACK_KDOS="${KDOS_PACK_KDOS:-0}" \
    KDOS_MAKE_BINHOST="${KDOS_MAKE_BINHOST:-0}" \
    KDOS_ISO_COMP="${KDOS_ISO_COMP:-}" \
    KDOS_CCACHE="${KDOS_CCACHE:-1}" \
    KPKG_KEEP_CACHE="${KDOS_MAKE_BINHOST:-0}" \
    KDOS_PKG_STORE="${KDOS_PKG_STORE:-0}" \
    KDOS_PKG_STORE_MAX="${KDOS_PKG_STORE_MAX:-60G}" \
    KPKG_STORE="$_store" \
    KPKG_STORE_CHECK="$_store_check" \
    KPKG_STORE_SALT="${KPKG_STORE_SALT:-}" \
    PATH=/usr/bin:/usr/sbin:/bin:/sbin:/usr/local/bin \
    /bin/bash -c "cd /kdos && exec \"\$@\"" -- "$@"

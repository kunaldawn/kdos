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
# Materialize home directories for the human users declared in fs/etc/passwd.
# The fs/ overlay is copied in phase 1, before shadow exists, and git cannot
# carry ownership — so the homes are created here, in the chroot, from
# /etc/skel. Idempotent: existing files are left alone.

set -e
source script/packaging.env.sh

echo "Creating user home directories..."

while IFS=: read -r name _pw uid gid _gecos home shell; do
    case "$uid" in
        ''|*[!0-9]*) continue ;;
    esac
    [ "$uid" -ge 1000 ] || continue
    [ "$uid" -lt 65534 ] || continue
    [ -n "$home" ] || continue

    echo "  $name ($uid:$gid) -> $home"
    mkdir -p "$home"
    # Skel wins: this only ever runs against the build tree, where the home is
    # a build artifact. With no-clobber, editing fs/etc/skel and rebuilding
    # would silently ship the previous build's dotfiles instead.
    if [ -d /etc/skel ]; then
        cp -r /etc/skel/. "$home"/ 2>/dev/null || true
    fi
    chown -R "$uid:$gid" "$home"
    chmod 0700 "$home"
done < /etc/passwd

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
    #
    # cp -r overwrites but never DELETES, so a file skel has since dropped
    # lingers in the home forever — a 256px icon from an older kdos-icons rode
    # three rebuilds that way. The generated theme trees are wholly build
    # output (00_theme.sh regenerates them from scratch every run) and nothing
    # user-authored lives there, so clear them first rather than merging onto
    # whatever the last build left.
    rm -rf "$home/.icons" "$home/.themes"
    if [ -d /etc/skel ]; then
        cp -r /etc/skel/. "$home"/ 2>/dev/null || true
    fi

    # XDG user dirs. ~/.config/user-dirs.dirs names them, but git cannot carry
    # an empty directory through /etc/skel, so they are created here. niri's
    # screenshot-path points into Pictures/Screenshots and will not create the
    # tree itself.
    for _d in Desktop Downloads Documents Music Pictures Pictures/Screenshots \
              Videos Public Templates .local/bin .local/share/applications; do
        mkdir -p "$home/$_d"
    done

    chown -R "$uid:$gid" "$home"
    chmod 0700 "$home"
done < /etc/passwd

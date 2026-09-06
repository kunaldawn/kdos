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

# THE FILES THAT CARRY A PASSWORD ARE 0600, AND GIT CANNOT SAY SO: it records
# the execute bit and nothing else, so phase 1's replay lands them 644. msmtp
# refuses to send through an .msmtprc that carries a `password` line and is not
# 600, and mbsync's PassCmd output is a password by definition. Skel is fixed
# here rather than in phase 1 because a port may own one of these paths too,
# and phase 4 has run by now.
#
# aerc's accounts.conf IS NOT IN THIS LIST because it is not shipped: aerc
# refuses to start on one that group or other can read, and its own wizard
# writes the file at 0600 on first run.
for _s in .msmtprc .mbsyncrc; do
    [ -f "/etc/skel/$_s" ] && chmod 600 "/etc/skel/$_s"
done

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
    # Desktop state the home accumulated from a generator that no longer
    # exists. Same reasoning as .icons/.themes: the home is MATERIALIZED from
    # skel, so anything skel has stopped providing has to be cleared here or
    # it outlives the thing that made it. The fs-manifest guard cannot help —
    # it only owns paths fs/ itself provided.
    rm -rf "$home/.config/cosmic"
    # The KDE colour scheme is generated output like .icons and .themes, so it
    # is cleared for the same reason: an accent renamed or dropped upstream
    # would otherwise leave a stale .colors file offered in every KDE app's
    # colour picker forever. kdeglobals is NOT cleared — KDE apps write their
    # own settings into it and `kdos theme` merges rather than overwrites.
    rm -rf "$home/.local/share/color-schemes"
    # The compositor config dir is skel's wholesale (rc.xml). A file skel has
    # stopped providing must not outlive it — a stale `autostart` here started
    # a second kdos-shell beside the supervised one and lost kdos-notifyd the
    # bus-name race on every boot.
    rm -rf "$home/.config/kdos-comp"
    # Same reason, and the alien launchers need it most: they have been named
    # kdos-<id> and <upstream-id> at different times, so a merge leaves both
    # and the app library shows every alien app twice.
    rm -rf "$home/.local/share/applications"
    if [ -d /etc/skel ]; then
        cp -r /etc/skel/. "$home"/ 2>/dev/null || true
    fi

    # XDG user dirs. ~/.config/user-dirs.dirs names them, but git cannot carry
    # an empty directory through /etc/skel, so they are created here.
    # `kdos-shot` writes into Pictures/Screenshots and will not create the tree
    # itself. Mail is here for the same reason from the other direction:
    # notmuch's mail_root and mbsync's MaildirStore both name ~/Mail, and both
    # report an error on a directory that is not there rather than making one.
    for _d in Desktop Downloads Documents Mail Music Pictures \
              Pictures/Screenshots Videos Public Templates .local/bin \
              .local/share/applications; do
        mkdir -p "$home/$_d"
    done

    # The same two, in the home. `cp` gives a NEW file skel's mode but leaves
    # an EXISTING one alone, so a home materialised by an earlier build still
    # has them at 644 and the first password written into one would be refused.
    for _s in .msmtprc .mbsyncrc; do
        [ -f "$home/$_s" ] && chmod 600 "$home/$_s"
    done

    chown -R "$uid:$gid" "$home"
    chmod 0700 "$home"
done < /etc/passwd

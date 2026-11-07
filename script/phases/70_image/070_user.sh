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
# The fs/ overlay is copied in 10_bootstrap, before shadow exists, and git cannot
# carry ownership — so the homes are created here, in the chroot, from
# /etc/skel. Rerunnable: skel overwrites every file it provides (see below).

set -e
source script/phases/70_image/phase.env

echo "Creating user home directories..."

# THE FILES THAT CARRY A PASSWORD ARE 0600, AND GIT CANNOT SAY SO: it records
# the execute bit and nothing else, so 10_bootstrap's replay lands them 644. msmtp
# refuses to send through an .msmtprc that carries a `password` line and is not
# 600, and mbsync's PassCmd output is a password by definition. Skel is fixed
# here rather than in 10_bootstrap because a port may own one of these paths too,
# and every package phase has run by now.
#
# aerc's accounts.conf IS NOT IN THIS LIST because it is not shipped: aerc
# refuses to start on one that group or other can read, and its own wizard
# writes the file at 0600 on first run.
for _s in .msmtprc .mbsyncrc; do
    [ -f "/etc/skel/$_s" ] && chmod 600 "/etc/skel/$_s"
done

# The fs-manifest guard removes FILES, never the directory a removed file
# emptied, and skel is copied wholesale into every home below — so an empty
# config directory here is recreated in every home, where nothing reads it and
# an audit counts it as live. git cannot carry an empty directory through fs/,
# so one under skel's .config is never something fs/ asked for. Only .config:
# the generated icon and theme trees legitimately contain empty leaves.
if [ -d /etc/skel/.config ]; then
    find /etc/skel/.config -mindepth 1 -type d -empty -delete
fi

# THE BACKUP COPIES OF THE ACCOUNT FILES DO NOT SHIP. shadow's tools write
# /etc/passwd-, group-, shadow- and gshadow- beside each file they change, and
# the build's changes carry the live accounts' password hashes. In system.sfs
# they would reach every installed system through the installer's copy, which
# replaces only the files themselves.
rm -f /etc/passwd- /etc/group- /etc/shadow- /etc/gshadow-

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
    # lingers in the home forever. The alien launchers need clearing most: a
    # launcher whose file name changes would stay under its old name as well,
    # and the app library would show that app twice.
    rm -rf "$home/.local/share/applications"
    if [ -d /etc/skel ]; then
        cp -r /etc/skel/. "$home"/ 2>/dev/null || true
    fi

    # XDG user dirs: the folders /etc/xdg/user-dirs.defaults lists. The first
    # Plasma login's xdg-user-dirs-update writes ~/.config/user-dirs.dirs over
    # them; git cannot carry an empty directory through /etc/skel, so they are
    # created here and Dolphin's Places has them before that login. Spectacle
    # saves into Pictures/Screenshots, made here so the tree exists before its
    # first capture. Mail is here because
    # the tools that use it do not make it:
    # notmuch's mail_root and mbsync's MaildirStore both name ~/Mail, and both
    # report an error on a directory that is not there rather than making one.
    # The calendar and contact stores are here for the same reason Mail is:
    # khal's shipped config globs `.local/share/calendars/*` and khard's names
    # `.local/share/contacts/personal`, and a store that is a plain directory
    # of files is a store `cp -r` backs up. khal's `discover` type does NOT
    # create what it is pointed at — deliberately, so a login grows nothing —
    # which is exactly why they are made here.
    for _d in Desktop Downloads Documents Mail Music Pictures \
              Pictures/Screenshots Projects Videos Public Templates .local/bin \
              .local/share/applications .local/share/calendars \
              .local/share/contacts/personal; do
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

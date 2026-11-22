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
# Stamp /etc/os-release with the commit this image was built from:
#
#   BUILD_ID="<short commit>[-dirty]"   os-release's own field for an image
#   KDOS_BUILD_DATE="<YYYY-MM-DD>"      that commit's date
#
# The COMMIT'S date and not the clock's: the image is reproducible from its
# tree, and a wall-clock stamp would make two builds of one commit differ in
# one file. `-dirty` says the tree had changes git had not recorded, so the
# commit alone does not reproduce it.
#
# The values come from the Makefile through script/chroot/exec.sh. A build run
# without them (kdosbuild.sh called by hand) writes neither line, and
# About this System shows the image as unstamped rather than inventing a commit.
#
# THE RELEASE RECORD, /etc/kdos-release, is written beside it: VERSION,
# BUILD_ID, BUILD_DATE, TREE
# (the commit) and TREE_TIME (the commit's time). `kdos update` judges an
# offered slot image newer by TREE_TIME against the image's own time, and
# kdos-bootctl copies the record to the ESP for the boot menu; without it no
# image is ever newer than the running system. A build with no commit writes
# none. VERSION has one form, the one `kdos update` writes when it fills a
# slot from a tree: a release build gives the release without its tag's `v`
# (0.2.1), any other build os-release's VERSION_ID and `+BUILD_ID`
# (0.2.1+4dc1f7f82-dirty), the name kdos-bootctl and kdos-sysupdated also
# give a root without a record. Two forms would name the two slots of one
# machine differently in System Versions and on the logout screen.
#
# A RELEASE BUILD (KDOS_GIT_RELEASE set: a clean checkout with a release tag
# on HEAD) stops when the tag without its `v` is not the root's
# /etc/os-release VERSION_ID: every surface that names the release (About
# this System, `kdos version`, the boot menus, What's New, /etc/kdos-release)
# reads it from there, so an unbumped file would ship the new tag calling
# itself the old release. The root's copy is the one 10_bootstrap's
# 000_file_system.sh made of fs/etc/os-release, so a bump reaches a tree
# already built only when that step runs again.
#
# Runs before 100_packs.sh so the base pack carries the stamped file.

set -e
source script/phases/70_image/phase.env

OSR=/etc/os-release

# Any earlier stamp goes first: this phase is re-run on the tree it finds.
grep -v -e '^BUILD_ID=' -e '^KDOS_BUILD_DATE=' "$OSR" > "$OSR.new" || true
cat "$OSR.new" > "$OSR"
rm -f "$OSR.new"

vid=$(sed -n 's/^VERSION_ID="\{0,1\}\([^"]*\)"\{0,1\}$/\1/p' "$OSR")
if [ -n "${KDOS_GIT_RELEASE:-}" ] && [ "${KDOS_GIT_RELEASE#v}" != "$vid" ]; then
    echo "FATAL: release $KDOS_GIT_RELEASE but /etc/os-release says VERSION_ID=$vid — bump fs/etc/os-release, or run 10_bootstrap:000_file_system.sh again if it is bumped" >&2
    exit 1
fi

rm -f /etc/kdos-release
if [ -z "${KDOS_GIT_COMMIT:-}" ]; then
    echo "No commit given; $OSR left unstamped."
    exit 0
fi

id="$KDOS_GIT_COMMIT"
[ "${KDOS_GIT_DIRTY:-0}" = 1 ] && id="$id-dirty"
echo "BUILD_ID=\"$id\"" >> "$OSR"
[ -n "${KDOS_GIT_DATE:-}" ] && echo "KDOS_BUILD_DATE=\"$KDOS_GIT_DATE\"" >> "$OSR"
echo "Stamped $OSR: BUILD_ID=$id${KDOS_GIT_DATE:+, KDOS_BUILD_DATE=$KDOS_GIT_DATE}"

ver="$vid+$id"
[ -n "${KDOS_GIT_RELEASE:-}" ] && ver="${KDOS_GIT_RELEASE#v}"

{
    echo '# The KDOS release this root holds. Written by the image build and by'
    echo '# `kdos update` when it fills this root; read by kdos-sysupdated and'
    echo '# kdos-bootctl.'
    echo "VERSION=\"$ver\""
    echo "BUILD_ID=\"$id\""
    echo "BUILD_DATE=\"${KDOS_GIT_DATE:-}\""
    echo "TREE=\"$KDOS_GIT_COMMIT\""
    echo "TREE_TIME=\"${KDOS_GIT_TIME:-0}\""
    echo "INSTALLED_BY=\"medium\""
} > /etc/kdos-release
chmod 0644 /etc/kdos-release
echo "Wrote /etc/kdos-release: $ver, tree time ${KDOS_GIT_TIME:-0}"

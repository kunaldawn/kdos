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
# kdos-about shows the image as unstamped rather than inventing a commit.
#
# Runs before 100_packs.sh so the base pack carries the stamped file.

set -e
source script/phases/70_image/phase.env

OSR=/etc/os-release

# Any earlier stamp goes first: this phase is re-run on the tree it finds.
grep -v -e '^BUILD_ID=' -e '^KDOS_BUILD_DATE=' "$OSR" > "$OSR.new" || true
cat "$OSR.new" > "$OSR"
rm -f "$OSR.new"

if [ -z "${KDOS_GIT_COMMIT:-}" ]; then
    echo "No commit given; $OSR left unstamped."
    exit 0
fi

id="$KDOS_GIT_COMMIT"
[ "${KDOS_GIT_DIRTY:-0}" = 1 ] && id="$id-dirty"
echo "BUILD_ID=\"$id\"" >> "$OSR"
[ -n "${KDOS_GIT_DATE:-}" ] && echo "KDOS_BUILD_DATE=\"$KDOS_GIT_DATE\"" >> "$OSR"
echo "Stamped $OSR: BUILD_ID=$id${KDOS_GIT_DATE:+, KDOS_BUILD_DATE=$KDOS_GIT_DATE}"

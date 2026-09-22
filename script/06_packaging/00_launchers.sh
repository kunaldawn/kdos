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
# Reconcile the alien-app launchers against the packs this image actually
# carries.
#
# A LAUNCHER OUTLIVES ITS PACK UNLESS SOMETHING RECONCILES THEM, and nothing
# did. The generated set — `/etc/skel/.local/share/applications/*.desktop`,
# `/usr/share/kdos/alien-apps` and the `/usr/local/bin` shims — is written by
# `kdos-appbox genlaunchers`, and the `fs-manifest` guard cannot reach any of
# it: that guard only owns paths `fs/` itself provided, and these are generated
# here. So a medium that carries no applications still offered Firefox, GIMP,
# LibreOffice, KeePassXC and zathura in the Start menu, each tagged `[box]`,
# each dispatching to a pack the machine has never had.
#
# GENLAUNCHERS RECONCILES RATHER THAN APPENDS, which is why this is one call
# and not a deletion pass. It sweeps every `.desktop` carrying
# `X-KDOS-Alien=true`, every `/usr/local/bin` symlink it recognises as its own,
# and rewrites the table and the mime cache whole. Against a source with no
# packs in it that is exactly the clean-out wanted; against one with packs it
# is the population step. A second sweep written here would be a second answer
# to which launchers are ours, and the one nobody is looking at is the one that
# drifts.
#
# RUNS BEFORE 00_user.sh, and the ordering is load-bearing. That step
# materialises every home with `cp -r /etc/skel/.`, so a skel cleaned after it
# leaves the launchers in `/home/kdos` and the Start menu reads the home first.
# Lexicographic order does the sequencing.
#
# `--packs-dir` AND NOT `--packs`, because at packaging time there is no
# kdos-packd and no mount: the build is a chroot in an unprivileged container.
# The flag reads one extracted pack per subdirectory, named after the pack,
# which is the shape `kdos-pack image` + `fsck.erofs --extract` produce. The
# directory is empty today because nothing is baked; the day a pack is baked
# again this same call populates the set from it.

set -e
source script/packaging.env.sh

# The extraction root a bake would fill. Created empty rather than skipped when
# absent: the reconcile has to run on a tree with no packs — that is the case
# that leaves ghosts — and genlaunchers refuses a source directory that is not
# there.
EXTRACT="${KDOS_PACK_EXTRACT:-/var/tmp/kdos-pack-extract}"
mkdir -p "$EXTRACT"

if ! command -v kdos-appbox >/dev/null 2>&1; then
    echo "[launchers] kdos-appbox is not installed — skipping" >&2
    exit 0
fi

echo "[launchers] reconciling alien launchers against $EXTRACT"
kdos-appbox genlaunchers --packs-dir "$EXTRACT" /

# THE TABLE IS THE ASSERTION, not the exit status. genlaunchers reports a
# launcher count on stderr and exits 0 whether or not it wrote anything, so a
# run that found the wrong source directory looks identical to a correct one.
# A table that is header-only is what "no packs" has to look like.
TABLE=/usr/share/kdos/alien-apps
test -s "$TABLE"
rows=$(grep -vc '^#' "$TABLE" || true)
entries=$(find /etc/skel/.local/share/applications -name '*.desktop' \
          -exec grep -l 'X-KDOS-Alien=true' {} + 2>/dev/null | wc -l)
echo "[launchers] $rows table row(s), $entries alien desktop entr(y|ies)"

# The two have to agree. A row with no entry is a shim nothing can launch from
# a menu; an entry with no row is the ghost this step exists to remove.
if [ "$rows" -eq 0 ] && [ "$entries" -ne 0 ]; then
    echo "FATAL: $entries alien desktop entries survived a reconcile that" >&2
    echo "       found no packs. genlaunchers did not sweep them." >&2
    exit 1
fi

rmdir "$EXTRACT" 2>/dev/null || true

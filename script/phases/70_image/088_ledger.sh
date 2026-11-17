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
# Write the image ledger: every package the image carries, its bytes and why
# it is there, to build/image-ledger.
#
# THE LEDGER IS script/lib/imageset.py --ledger over this root: one line per
# installed package, largest first, with its bytes, its MiB and the shortest
# dependency path from a ports/base seed to it (`-` for a package outside the
# base, which only a KDOS_IMAGE=full image carries), then a `total` line. Each
# inode is counted once, so the total is the bytes of package files the image
# compresses; files no package owns (fs/ and the caches the steps generate)
# are not in it.
#
# RUNS AFTER 087_gates.sh, so what it measures is the tree the sweeps, the
# indexes and the gates left, the tree 110_iso.sh squashes. 110_iso.sh prints
# the ledger's top twenty when the ISO passes the release limit, so a build
# that stops there names what the image spends its bytes on. The file is
# written beside the build's other outputs, outside the image root, and
# replaced whole on every run.

set -e
source script/phases/70_image/phase.env

LEDGER=/kdos/build/image-ledger

python3 /kdos/script/lib/imageset.py --tree /kdos --root / --ledger --out $LEDGER
echo "[ledger] the twenty largest packages on the image ($LEDGER):"
head -n 21 $LEDGER
tail -n 1 $LEDGER

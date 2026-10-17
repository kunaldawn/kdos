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
# The checks on the finished tree, before it is carried into the image.
#
#   testing/unowned.py     files no package and no fs/ path provides. A
#                          report: the step does not stop on it.
#   testing/linkcheck.py   a DT_NEEDED owned outside the consumer's
#                          depends, an interpreter that does not resolve,
#                          libcrypto.so.3 and .so.4 in one process.
#   testing/entrycheck.py  a dangling symlink, a desktop or D-Bus Exec= that
#                          does not resolve, a visible Icon= with no icon, a
#                          udev program that does not exist.
#
# EVERY CHECK RUNS BEFORE THE STEP DECIDES, so one build prints every finding
# instead of the first check's. A finding of linkcheck or entrycheck stops the
# build here: the ISO is not written from a tree that fails them. Each needs
# only this phase to re-run once the recipe is fixed.
#
# RUNS AFTER the sweep and every index (the manual index, the font cache), so
# what it reads is what the image carries, and before 090_initramfs.sh and
# 110_iso.sh. What the later steps write into the tree (the initramfs and its
# module list, the source offer) is generated, and unowned.py subtracts it.
# The scripts read the repository's phase lists and recipes, so the working
# directory is /kdos.

set -e
source script/phases/70_image/phase.env

cd /kdos
rc=0

echo "[gates] unowned files"
python3 testing/unowned.py --root / --fs /kdos/fs || true

echo "[gates] run-time references"
python3 testing/linkcheck.py --root / || rc=1

echo "[gates] links, launchers and rules"
python3 testing/entrycheck.py --root / || rc=1

if [ "$rc" != 0 ]; then
    echo "FATAL: the tree fails linkcheck or entrycheck; see the FAIL and finding lines above." >&2
    exit 1
fi
echo "[gates] clean"

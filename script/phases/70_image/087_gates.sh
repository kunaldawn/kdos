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
#                          report, except under usr/share/{locale,man,doc,
#                          info}: 020_cleanup.sh has removed what the size
#                          and language policy prunes there, so a file left
#                          is unowned documentation that would ship.
#   testing/linkcheck.py   a DT_NEEDED owned outside the consumer's
#                          depends, an interpreter that does not resolve,
#                          libcrypto.so.3 and .so.4 in one process.
#   testing/entrycheck.py  a dangling symlink, a desktop or D-Bus Exec= that
#                          does not resolve, a visible Icon= with no icon, a
#                          udev program that does not exist.
#   script/lib/imageset.py --verify
#                          under KDOS_IMAGE=base, a kept package whose
#                          recorded run-time needs (`kpkg needs`: sonames,
#                          interpreters, QML and Python imports, `runtime =`
#                          words) only a package of /kdos/build/fs the stage
#                          dropped provides. It is the one check that sees a
#                          QML or Python import. A package installed before
#                          needs were recorded has no record and is counted
#                          as unmeasured, never failed; an edge nothing in
#                          either tree provides is counted, not failed.
#
# EVERY CHECK RUNS BEFORE THE STEP DECIDES, so one build prints every finding
# instead of the first check's. A finding of linkcheck, entrycheck or the
# base's needs, or an unowned file under those four directories, stops the
# build here: the ISO is not written from a tree that fails them. Each needs
# only this phase to re-run once the recipe or ports/base is fixed.
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
python3 testing/unowned.py --root / --fs /kdos/fs \
    --fail-under usr/share/locale --fail-under usr/share/man \
    --fail-under usr/share/doc --fail-under usr/share/info || rc=1

echo "[gates] run-time references"
python3 testing/linkcheck.py --root / || rc=1

echo "[gates] links, launchers and rules"
python3 testing/entrycheck.py --root / || rc=1

if [ "${KDOS_IMAGE:-base}" = full ]; then
    echo "[gates] the base's run-time needs: KDOS_IMAGE=full drops nothing"
else
    echo "[gates] the base's run-time needs"
    python3 script/lib/imageset.py --tree /kdos --root / --verify \
        --full /kdos/build/fs || rc=1
fi

if [ "$rc" != 0 ]; then
    echo "FATAL: the tree fails unowned, linkcheck, entrycheck or the base's run-time needs; see the FAIL and finding lines above." >&2
    exit 1
fi
echo "[gates] clean"

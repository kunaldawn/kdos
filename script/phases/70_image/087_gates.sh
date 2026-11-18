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
#   binhost N: lines       under KDOS_MAKE_BINHOST=1, a current package
#                          stanza of /kdos/build/binhost/PACKAGES (one with
#                          an E: and no O:) that carries no `N:` line: its
#                          package has no run-time needs record here, so
#                          `kdos update install` would keep every build-only
#                          dependency of it (010_binhost.sh). Fixed by
#                          rebuilding that port.
#
# EVERY CHECK RUNS BEFORE THE STEP DECIDES, so one build prints every finding
# instead of the first check's. A finding of linkcheck, entrycheck or the
# base's needs, a binhost stanza with no N:, or an unowned file under those four directories, stops the
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

if [ "${KDOS_MAKE_BINHOST:-0}" = 1 ] && [ -f /kdos/build/binhost/PACKAGES ]; then
    echo "[gates] the binhost's measured run-time needs"
    unmeasured=$(awk -v RS= -F'\n' '{
        p = ""; e = ""; o = 0; n = 0
        for (i = 1; i <= NF; i++) {
            if ($i ~ /^P:/) p = substr($i, 3)
            else if ($i ~ /^E:/) e = substr($i, 3)
            else if ($i ~ /^O:/) o = 1
            else if ($i ~ /^N:/) n = 1
        }
        if (p != "" && e != "" && !o && !n) print p
    }' /kdos/build/binhost/PACKAGES)
    if [ -n "$unmeasured" ]; then
        echo "FAIL $(wc -w <<<"$unmeasured") binhost package(s) carry no N: line (no run-time needs record): $(head -c 600 <<<"${unmeasured//$'\n'/ }")"
        rc=1
    fi
fi

if [ "$rc" != 0 ]; then
    echo "FATAL: the tree fails unowned, linkcheck, entrycheck, the base's run-time needs or the binhost's N: lines; see the FAIL and finding lines above." >&2
    exit 1
fi
echo "[gates] clean"

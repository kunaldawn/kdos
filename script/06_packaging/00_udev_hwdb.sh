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
# Compile the udev hardware database into /etc/udev/hwdb.bin.
#
# kpkg's hwdb trigger keeps the trie current on every install and removal.
# This step rebuilds it from scratch over the finished tree and asserts on the
# result, so the image never ships a trie a partial build left behind.
#
# EUDEV SHIPS hwdb.d AS TEXT AND INSTALLS NO TRIE, and half its rules open with
# `IMPORT{builtin}="hwdb ..."`. That import returns nothing at all when the
# binary is absent, and nothing logs it above debug level: 60-keyboard.hwdb's
# laptop brightness, wifi and volume quirks never apply, 60-evdev.hwdb's
# EVDEV_ABS_* overrides never reach libinput so it sizes those touchpads from
# the wrong axis ranges, and ID_VENDOR_FROM_DATABASE / ID_MODEL_FROM_DATABASE
# are missing from every PCI and USB device. The machine reads as hardware with
# no quirks and no names rather than as a missing file.
#
# /etc/udev/hwdb.bin AND NOT `--usr`. libudev reads that path first and
# `$UDEV_LIBEXEC_DIR/hwdb.bin` second, and this port's --with-rootlibdir puts
# the second at /lib/udev — which is /usr/lib/udev, since /lib is a symlink to
# usr/lib. 01_initramfs.sh copies /usr/lib/udev wholesale, so a trie written
# there is ~10 MB of RAM on every boot for an early stage that imports no hwdb
# property at all.
#
# RUNS AFTER 00_orphans.sh, so the hwdb.d files of a swept package are not
# indexed, and before 01_initramfs.sh and 02_iso.sh, which carry the tree into
# the image. Lexicographic order does the sequencing.

set -e
source script/packaging.env.sh

# The two directories udevadm searches, in its own order. Named here only to
# report what was found — udevadm is not told where to look.
DIRS="/etc/udev/hwdb.d /lib/udev/hwdb.d"
BIN=/etc/udev/hwdb.bin

if ! command -v udevadm >/dev/null 2>&1; then
    echo "[hwdb] udevadm is not installed — skipping" >&2
    exit 0
fi

# THE SOURCE COUNT IS THE ASSERTION, NOT THE EXIT STATUS AND NOT THE FILE.
# eudev's `udevadm hwdb --update` has no empty-input shortcut: given no *.hwdb
# to read it still stores an 80-byte header for an empty trie and exits 0. A
# step that trusted either would report success over an image on which every
# `IMPORT{builtin}="hwdb ..."` returns nothing.
sources=$(find $DIRS -name '*.hwdb' 2>/dev/null | wc -l)
if [ "$sources" -eq 0 ]; then
    echo "FATAL: no *.hwdb under $DIRS;" >&2
    echo "       check that eudev installed its hwdb.d." >&2
    exit 1
fi

echo "[hwdb] compiling $BIN from $sources source file(s)"

# THE STALE TRIE GOES FIRST. The build tree is incremental, so the file
# measured below is this run's product rather than an earlier build's.
rm -f "$BIN"
udevadm hwdb --update

if [ ! -s "$BIN" ]; then
    echo "FATAL: $BIN was not written from $sources source file(s)." >&2
    exit 1
fi

echo "[hwdb] $BIN: $(wc -c < "$BIN") bytes"

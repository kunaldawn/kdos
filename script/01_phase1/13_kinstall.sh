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

# kinstall is C with no library dependencies at all — not even ncurses — so it
# cross-compiles here in phase 1, against nothing but musl and the kernel
# headers, and exists on every tree from the first bootable image onward.
#
# libkbase, libktui and libkcolor are ours and link nothing either, so the
# terminal toolkit costs that property nothing: their sources simply join the
# same command line. Keep it that way. A library that needs a real -l moves
# this build to phase 4 and takes the installer off every tree before it.
#
# libkcolor is on this line because libktui's ktui_theme.c includes kcolor.h —
# the palette numbers live there and nowhere else. Leaving it out builds fine
# on the host, where testing/selftest.sh passes it, and fails only here.

set -e
source script/phase1.env.sh
source script/util/port.sh

if [ -f "$MARK/kinstall" ] && [ "${KDOS_REPLAY:-0}" != "1" ]; then
    exit 0
fi

SRC=$WORKSPACE/src/packages/kdos-installer
LIBS=$WORKSPACE/src/libs
OUT=$BUILD_DIR/tmp/kinstall

# THE CATALOGUE READER IS COMPILED IN, not shelled out to. The installer runs
# before anything it would call exists, on the first bootable image, so it reads
# the catalogue itself; catalogue.c links against kb_* alone, so carrying it
# costs no library this program does not already have. Its header lives with
# kdos-appbox, which is why that directory is on the include path and is not a
# dependency on the rest of that program.
#
# EVERY .c IN $SRC IS COMPILED, by glob and never by name.
# src/packages/kdos-installer/build.sh builds this same program from this same
# directory as a port, and the two must compile the same sources — only the
# flags differ. A name list in either place omits the next file added on one
# side alone, and the phase-1 kinstall and the packaged one stop being the same
# program, with only a link error or a silently absent page to say so.
$KDOS_TARGET-gcc \
    -O2 -pipe -std=gnu11 -D_GNU_SOURCE -Wall -Wextra \
    -I"$LIBS"/libkbase -I"$LIBS"/libktui -I"$LIBS"/libkcolor -I"$SRC" \
    -I"$WORKSPACE"/src/packages/kdos-appbox \
    -o "$OUT" \
    "$SRC"/*.c \
    "$WORKSPACE"/src/packages/kdos-appbox/catalogue.c \
    "$LIBS"/libkbase/*.c "$LIBS"/libktui/*.c "$LIBS"/libkcolor/*.c

install -Dm755 "$OUT" $SYSROOT/usr/bin/kinstall

touch "$MARK/kinstall"

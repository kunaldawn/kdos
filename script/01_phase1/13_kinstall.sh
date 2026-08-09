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
# libkbase, libktui and libkcolor are ours and link nothing either, so pulling
# the terminal toolkit out of the installer did not cost that property: the
# sources simply join the same command line. Keep it that way. A library that
# needs a real -l moves this build to phase 4 and takes the installer off
# every tree before it.
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

$KDOS_TARGET-gcc \
    -O2 -pipe -std=gnu11 -D_GNU_SOURCE -Wall -Wextra \
    -I"$LIBS"/libkbase -I"$LIBS"/libktui -I"$LIBS"/libkcolor -I"$SRC" \
    -o "$OUT" \
    "$SRC"/main.c "$SRC"/probe.c "$SRC"/conf.c \
    "$SRC"/install.c "$SRC"/pages.c \
    "$LIBS"/libkbase/*.c "$LIBS"/libktui/*.c "$LIBS"/libkcolor/*.c

install -Dm755 "$OUT" $SYSROOT/usr/bin/kinstall

touch "$MARK/kinstall"

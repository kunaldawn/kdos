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

set -e
source script/phase1.env.sh
source script/util/port.sh

if [ -f "$MARK/kinstall" ] && [ "${KDOS_REPLAY:-0}" != "1" ]; then
    exit 0
fi

SRC=$WORKSPACE/src/kinstall
OUT=$BUILD_DIR/tmp/kinstall

$KDOS_TARGET-gcc \
    -O2 -pipe -std=gnu11 -D_GNU_SOURCE -Wall -Wextra \
    -o "$OUT" \
    "$SRC"/main.c "$SRC"/util.c "$SRC"/term.c "$SRC"/draw.c \
    "$SRC"/input.c "$SRC"/ui.c "$SRC"/probe.c "$SRC"/conf.c \
    "$SRC"/install.c "$SRC"/pages.c

install -Dm755 "$OUT" $SYSROOT/usr/bin/kinstall

touch "$MARK/kinstall"

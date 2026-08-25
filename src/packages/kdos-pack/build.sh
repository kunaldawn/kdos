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

# It ships as well as running on the host, because `kdos-box freeze` builds a
# pack out of a box's writable layer on the machine somebody is working on —
# and mkfs.erofs and zstd are both on the target.
LIBS="$PORT_SRC/../../libs"

gcc $CFLAGS -O2 -std=gnu11 -D_GNU_SOURCE -Wall -Wextra \
	-I"$LIBS/libkbase" -I"$LIBS/libksig" -I"$LIBS/libkpkg" \
	-I"$LIBS/libkpack" \
	-o kdos-pack "$PORT_SRC"/main.c \
	"$LIBS"/libkbase/*.c "$LIBS"/libksig/*.c \
	"$LIBS"/libksig/monocypher/*.c "$LIBS"/libkpkg/*.c \
	"$LIBS"/libkpack/*.c $LDFLAGS

install -Dm755 kdos-pack "$PKG/usr/bin/kdos-pack"

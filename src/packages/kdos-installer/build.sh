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

LIBS="$PORT_SRC/../../libs"

# THE CATALOGUE READER IS COMPILED IN, not shelled out to. It uses kb_* alone,
# so it costs none of the libraries this program deliberately does not link —
# three is what lets kinstall live in phase 1 and exist on every tree from the
# first bootable image. A live installer also cannot assume anything is on
# $PATH in the target it is building.
APPBOX="$PORT_SRC/../kdos-appbox"

# EVERY .c IN THIS DIRECTORY IS COMPILED, by glob and never by name.
# script/01_phase1/13_kinstall.sh builds this same program from this same
# directory before any packages.txt exists, and the two must compile the same
# sources — only the flags differ. A name list in either place omits the next
# file added on one side alone, and the phase-1 kinstall and the packaged one
# stop being the same program, with only a link error or a silently absent page
# to say so. The glob is exact here: the directory holds nothing generated and
# no test harness.
gcc $CFLAGS -O2 -std=gnu11 -D_GNU_SOURCE -Wall -Wextra \
	-I"$LIBS/libkbase" -I"$LIBS/libktui" -I"$LIBS/libkcolor" \
	-I"$PORT_SRC" -I"$APPBOX" \
	-o kinstall \
	"$PORT_SRC"/*.c \
	"$APPBOX"/catalogue.c \
	"$LIBS"/libkbase/*.c "$LIBS"/libktui/*.c "$LIBS"/libkcolor/*.c $LDFLAGS

install -Dm755 kinstall "$PKG/usr/bin/kinstall"

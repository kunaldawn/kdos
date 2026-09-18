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

gcc $CFLAGS -O2 -std=gnu11 -D_GNU_SOURCE -Wall -Wextra \
	-I"$LIBS/libkbase" -I"$LIBS/libktui" -I"$LIBS/libkcolor" \
	-I"$PORT_SRC" -I"$APPBOX" \
	-o kinstall \
	"$PORT_SRC"/main.c "$PORT_SRC"/probe.c "$PORT_SRC"/conf.c \
	"$PORT_SRC"/install.c "$PORT_SRC"/pages.c "$PORT_SRC"/dump.c \
	"$APPBOX"/catalogue.c \
	"$LIBS"/libkbase/*.c "$LIBS"/libktui/*.c "$LIBS"/libkcolor/*.c $LDFLAGS

install -Dm755 kinstall "$PKG/usr/bin/kinstall"

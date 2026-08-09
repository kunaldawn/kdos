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

gcc $CFLAGS -O2 -std=gnu11 -D_GNU_SOURCE -Wall -Wextra \
	-I"$LIBS/libkbase" -I"$LIBS/libktui" -I"$PORT_SRC" \
	-o kinstall \
	"$PORT_SRC"/main.c "$PORT_SRC"/probe.c "$PORT_SRC"/conf.c \
	"$PORT_SRC"/install.c "$PORT_SRC"/pages.c \
	"$LIBS"/libkbase/*.c "$LIBS"/libktui/*.c $LDFLAGS

install -Dm755 kinstall "$PKG/usr/bin/kinstall"

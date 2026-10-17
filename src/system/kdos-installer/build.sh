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

APPBOX="$PORT_SRC/../kdos-appbox"

gcc $CFLAGS -O2 -std=gnu11 -D_GNU_SOURCE -Wall -Wextra \
	-I"$LIBS/libkbase" -I"$LIBS/libktui" -I"$LIBS/libkcolor" \
	-I"$PORT_SRC" -I"$APPBOX" \
	-o kinstall \
	"$PORT_SRC"/*.c \
	"$APPBOX"/catalogue.c \
	"$LIBS"/libkbase/*.c "$LIBS"/libktui/*.c "$LIBS"/libkcolor/*.c $LDFLAGS

install -Dm755 kinstall "$PKG/usr/bin/kinstall"

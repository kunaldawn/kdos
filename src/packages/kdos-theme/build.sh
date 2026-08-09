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
	-I"$LIBS/libkbase" -I"$LIBS/libkcolor" -I"$PORT_SRC" \
	-o kdos-theme \
	"$PORT_SRC"/main.c "$PORT_SRC"/gtk.c "$PORT_SRC"/icons.c \
	"$PORT_SRC"/cursors.c \
	"$LIBS"/libkbase/*.c "$LIBS"/libkcolor/*.c $LDFLAGS

install -Dm755 kdos-theme "$PKG/usr/bin/kdos-theme"

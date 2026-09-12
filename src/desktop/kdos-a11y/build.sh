#!/bin/bash
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# A reader is a client of the console session and nothing more: it links the
# same four libraries every surface does and no synthesiser at all.
#
# espeak-ng IS A SUBPROCESS AND IS NEVER LINKED. It is GPL-3.0 and this program
# is not; a pipe keeps that question out of the binary. It is in `depends`
# because a reader with nothing to speak through says nothing.
LIBS="$PORT_SRC/../../libs"

gcc $CFLAGS -O2 -std=gnu11 -D_GNU_SOURCE -Wall -Wextra \
	-I"$PORT_SRC" \
	-I"$LIBS/libkbase" -I"$LIBS/libkcolor" -I"$LIBS/libktui" \
	-I"$LIBS/libkdisp" -I"$LIBS/libkcon" \
	-o kdos-a11y \
	"$PORT_SRC"/*.c \
	"$LIBS"/libkbase/*.c "$LIBS"/libkcolor/*.c "$LIBS"/libktui/*.c \
	"$LIBS"/libkdisp/*.c "$LIBS"/libkcon/*.c \
	$LDFLAGS

install -Dm755 kdos-a11y "$PKG/usr/bin/kdos-a11y"

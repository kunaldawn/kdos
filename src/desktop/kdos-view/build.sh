#!/bin/bash
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# The display half, and the only thing on the console path that needs a GPU
# device. libkkms brings drm, input, seat and a font renderer; keeping it here
# rather than in the session is what lets kdos-con link none of it and come up
# on a machine whose driver does not.
#
# KDOS_VIEW_KMS is what compiles that mode in, and KDOS_VIEW_CAST the recording
# one. A build without either still has the tty and dump modes, which is how the
# self-test checks the console's goldens on a machine with none of these
# libraries.
#
# A RECORDING IS A VIEW NOBODY LOOKS AT: it rasterises through the same cell
# painter and writes into a PipeWire stream instead of onto a screen, which is
# why PipeWire is here and not in the session.
LIBS="$PORT_SRC/../../libs"
# libpng is `--shot`'s: one frame of the composited grid, written as a picture
# rather than as the text a `--dump` prints.
PKGCFG="libdrm libinput libseat xkbcommon libudev fcft pixman-1 libpipewire-0.3 libpng"

gcc $CFLAGS -O2 -std=gnu11 -D_GNU_SOURCE -Wall -Wextra \
	-DKDOS_VIEW_VERSION="\"$version\"" -DKDOS_VIEW_KMS -DKDOS_VIEW_CAST \
	-DKDOS_VIEW_SHOT \
	-I"$PORT_SRC" \
	-I"$LIBS/libkbase" -I"$LIBS/libkcolor" -I"$LIBS/libktui" \
	-I"$LIBS/libkdisp" -I"$LIBS/libkcon" -I"$LIBS/libkcell" \
	-I"$LIBS/libkkms" \
	$(pkg-config --cflags $PKGCFG) \
	-o kdos-view \
	"$PORT_SRC"/*.c \
	"$LIBS"/libkbase/*.c "$LIBS"/libkcolor/*.c "$LIBS"/libktui/*.c \
	"$LIBS"/libkdisp/*.c "$LIBS"/libkcon/*.c "$LIBS"/libkcell/*.c \
	"$LIBS"/libkkms/*.c \
	$(pkg-config --libs $PKGCFG) $LDFLAGS

install -Dm755 kdos-view "$PKG/usr/bin/kdos-view"

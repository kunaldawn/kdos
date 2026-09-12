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

# NO WAYLAND, NO fcft, NO pixman. This is the half of the console desktop that
# holds the session and draws nothing: it rasterises no glyph and opens no
# device, which is why it still comes up on a machine whose GPU driver does
# not. The display is kdos-view's, and it links all of that.
#
# ncurses is in `depends` for its terminfo, not to link: the children get
# TERM=xterm-256color and read the entry from the database.
LIBS="$PORT_SRC/../../libs"

# kembed.h is a header with NO CODE, shared with kdos-cage: the private channel
# between this session and the compositor it forks for an embedded window. It is
# a struct definition and nothing else precisely so neither port grows a
# dependency on the other — kdos-cage links no KDOS library but libkcolor, and
# nothing Wayland is linked here.
CAGE="$PORT_SRC/../kdos-cage"

# Every .c in the port, by glob rather than by a list — a list is a second
# place to remember a new file, and the self-test globs.
gcc $CFLAGS -O2 -std=gnu11 -D_GNU_SOURCE -Wall -Wextra \
	-DKDOS_CON_VERSION="\"$version\"" \
	-I"$PORT_SRC" \
	-I"$LIBS/libkbase" -I"$LIBS/libkcolor" -I"$LIBS/libktui" \
	-I"$LIBS/libkdisp" -I"$LIBS/libkcon" -I"$LIBS/libkvt" \
	-I"$LIBS/libkwm" -I"$LIBS/libkxdg" -I"$CAGE" \
	-o kdos-con \
	"$PORT_SRC"/*.c \
	"$LIBS"/libkbase/*.c "$LIBS"/libkcolor/*.c "$LIBS"/libktui/*.c \
	"$LIBS"/libkdisp/*.c "$LIBS"/libkcon/*.c "$LIBS"/libkvt/*.c \
	"$LIBS"/libkwm/*.c "$LIBS"/libkxdg/*.c \
	$LDFLAGS

install -Dm755 kdos-con "$PKG/usr/bin/kdos-con"

# THREE NAMES, ONE BINARY, dispatched on argv[0] as ksvc and kdos-appbox are.
# kdos-grid starts a session here and attaches a view to it; kdos-con-login is
# what /etc/inittab reaches through kdos-getty.
ln -sf kdos-con "$PKG/usr/bin/kdos-grid"
install -d "$PKG/usr/local/sbin"
ln -sf ../../bin/kdos-con "$PKG/usr/local/sbin/kdos-con-login"

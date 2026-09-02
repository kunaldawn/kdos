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

# ONE BINARY, TWO DESKTOPS. libkdisp picks between libkwl and libkcon at
# startup, so the same source is an xdg-toplevel under kdos-comp and a cell
# surface under kdos-con.
#
# ncurses is in `depends` for its terminfo, not to link: the child gets
# TERM=xterm-256color and reads the entry from the database.
LIBS="$PORT_SRC/../../libs"

SCANNER="$(pkg-config --variable=wayland_scanner wayland-scanner)"
PROTO="$(pkg-config --variable=pkgdatadir wayland-protocols)"

# libkwl COMPILES WHOLE. A consumer that binds only xdg-shell still has to
# generate the layer-shell, session-lock, cursor-shape, tablet and
# primary-selection glue, because kwl.c references all of them
# unconditionally — which is why wlroots is in `depends` for a program that
# has no layer surface.
"$SCANNER" client-header "$PROTO/staging/cursor-shape/cursor-shape-v1.xml" \
	cursor-shape-v1-client-protocol.h
"$SCANNER" private-code  "$PROTO/staging/cursor-shape/cursor-shape-v1.xml" \
	cursor-shape-v1-protocol.c
"$SCANNER" private-code  "$PROTO/unstable/tablet/tablet-unstable-v2.xml" \
	tablet-unstable-v2-protocol.c
"$SCANNER" client-header \
	"$PROTO/unstable/primary-selection/primary-selection-unstable-v1.xml" \
	primary-selection-unstable-v1-client-protocol.h
"$SCANNER" private-code \
	"$PROTO/unstable/primary-selection/primary-selection-unstable-v1.xml" \
	primary-selection-unstable-v1-protocol.c
"$SCANNER" client-header "$PROTO/stable/xdg-shell/xdg-shell.xml" \
	xdg-shell-client-protocol.h
"$SCANNER" private-code  "$PROTO/stable/xdg-shell/xdg-shell.xml" \
	xdg-shell-protocol.c
"$SCANNER" client-header \
	"$PROTO/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml" \
	xdg-decoration-unstable-v1-client-protocol.h
"$SCANNER" private-code \
	"$PROTO/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml" \
	xdg-decoration-unstable-v1-protocol.c
"$SCANNER" client-header "$PROTO/staging/ext-session-lock/ext-session-lock-v1.xml" \
	ext-session-lock-v1-client-protocol.h
"$SCANNER" private-code  "$PROTO/staging/ext-session-lock/ext-session-lock-v1.xml" \
	ext-session-lock-v1-protocol.c
"$SCANNER" client-header \
	/usr/share/wlroots/protocols/wlr-layer-shell-unstable-v1.xml \
	wlr-layer-shell-unstable-v1-client-protocol.h
"$SCANNER" private-code \
	/usr/share/wlroots/protocols/wlr-layer-shell-unstable-v1.xml \
	wlr-layer-shell-unstable-v1-protocol.c

PKGCFG="fcft pixman-1 xkbcommon wayland-client libpng libjpeg libwebp libsixel"

gcc $CFLAGS -O2 -std=gnu11 -D_GNU_SOURCE -Wall -Wextra \
	-DKDOS_TERM_VERSION="\"$version\"" \
	-DHAVE_KIMG -DKIMG_HAVE_PNG -DKIMG_HAVE_JPEG -DKIMG_HAVE_WEBP \
	-DKIMG_HAVE_SIXEL \
	-I. -I"$PORT_SRC" \
	-I"$LIBS/libkbase" -I"$LIBS/libktui" -I"$LIBS/libkcolor" \
	-I"$LIBS/libkcell" -I"$LIBS/libkwl" -I"$LIBS/libkdisp" \
	-I"$LIBS/libkcon" -I"$LIBS/libkvt" -I"$LIBS/libkimg" \
	-I"$LIBS/libkxdg" \
	$(pkg-config --cflags $PKGCFG) \
	-o kdos-term \
	"$PORT_SRC"/*.c \
	"$LIBS"/libkwl/*.c "$LIBS"/libkdisp/*.c "$LIBS"/libkcon/*.c \
	"$LIBS"/libkcell/*.c "$LIBS"/libktui/*.c "$LIBS"/libkvt/*.c \
	"$LIBS"/libkimg/*.c "$LIBS"/libkcolor/*.c "$LIBS"/libkbase/*.c \
	"$LIBS"/libkxdg/*.c \
	./*-protocol.c \
	$(pkg-config --libs $PKGCFG) $LDFLAGS

install -Dm755 kdos-term "$PKG/usr/bin/kdos-term"
install -Dm644 "$PORT_SRC/kdos-term.desktop" \
	"$PKG/usr/share/applications/kdos-term.desktop"

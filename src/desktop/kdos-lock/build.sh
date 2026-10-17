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

SCANNER="$(pkg-config --variable=wayland_scanner wayland-scanner)"
PROTO="$(pkg-config --variable=pkgdatadir wayland-protocols)"

"$SCANNER" client-header "$PROTO/staging/cursor-shape/cursor-shape-v1.xml" \
	cursor-shape-v1-client-protocol.h
"$SCANNER" private-code  "$PROTO/staging/cursor-shape/cursor-shape-v1.xml" \
	cursor-shape-v1-protocol.c
"$SCANNER" private-code  "$PROTO/unstable/tablet/tablet-unstable-v2.xml" \
	tablet-unstable-v2-protocol.c
"$SCANNER" client-header "$PROTO/stable/viewporter/viewporter.xml" \
	viewporter-client-protocol.h
"$SCANNER" private-code  "$PROTO/stable/viewporter/viewporter.xml" \
	viewporter-protocol.c
"$SCANNER" client-header \
	"$PROTO/staging/fractional-scale/fractional-scale-v1.xml" \
	fractional-scale-v1-client-protocol.h
"$SCANNER" private-code \
	"$PROTO/staging/fractional-scale/fractional-scale-v1.xml" \
	fractional-scale-v1-protocol.c
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
"$SCANNER" client-header \
	"$PROTO/unstable/xdg-foreign/xdg-foreign-unstable-v2.xml" \
	xdg-foreign-unstable-v2-client-protocol.h
"$SCANNER" private-code \
	"$PROTO/unstable/xdg-foreign/xdg-foreign-unstable-v2.xml" \
	xdg-foreign-unstable-v2-protocol.c
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
"$SCANNER" client-header \
	/usr/share/wlroots/protocols/wlr-foreign-toplevel-management-unstable-v1.xml \
	wlr-foreign-toplevel-management-unstable-v1-client-protocol.h
"$SCANNER" private-code \
	/usr/share/wlroots/protocols/wlr-foreign-toplevel-management-unstable-v1.xml \
	wlr-foreign-toplevel-management-unstable-v1-protocol.c

PKGCFG="fcft fontconfig pixman-1 xkbcommon wayland-client"

gcc $CFLAGS -O2 -std=gnu11 -D_GNU_SOURCE -Wall -Wextra \
	-I. -I"$PORT_SRC" \
	-I"$LIBS/libkbase" -I"$LIBS/libktui" -I"$LIBS/libkcolor" -I"$LIBS/libkcell" -I"$LIBS/libkwl" -I"$LIBS/libkdisp" \
	$(pkg-config --cflags $PKGCFG) \
	-o kdos-lock \
	"$PORT_SRC"/main.c \
	"$LIBS"/libkwl/*.c "$LIBS"/libkdisp/*.c "$LIBS"/libkcell/*.c "$LIBS"/libktui/*.c "$LIBS"/libkcolor/*.c \
	"$LIBS"/libkbase/*.c \
	./*-protocol.c \
	$(pkg-config --libs $PKGCFG) $LDFLAGS

gcc $CFLAGS -O2 -std=gnu11 -D_GNU_SOURCE -Wall -Wextra \
	-o kdos-checkpass "$PORT_SRC"/checkpass.c -lcrypt $LDFLAGS

install -Dm755 kdos-lock "$PKG/usr/bin/kdos-lock"
install -Dm4755 kdos-checkpass "$PKG/usr/bin/kdos-checkpass"

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

# Two binaries from one port, and they are built SEPARATELY on purpose: the
# lock screen links libkwl and half of Wayland, while kdos-checkpass is setuid
# root and links libc and libcrypt and nothing else. Giving the setuid binary
# the graphics stack would be handing root a font parser.
LIBS="$PORT_SRC/../../libs"

SCANNER="$(pkg-config --variable=wayland_scanner wayland-scanner)"
PROTO="$(pkg-config --variable=pkgdatadir wayland-protocols)"

# cursor-shape: libkwl sets the pointer shape on enter, or the cursor
# vanishes over every chrome surface. The tablet protocol comes along because
# cursor-shape-v1's generated code references zwp_tablet_tool_v2.
"$SCANNER" client-header "$PROTO/staging/cursor-shape/cursor-shape-v1.xml" \
	cursor-shape-v1-client-protocol.h
"$SCANNER" private-code  "$PROTO/staging/cursor-shape/cursor-shape-v1.xml" \
	cursor-shape-v1-protocol.c
"$SCANNER" private-code  "$PROTO/unstable/tablet/tablet-unstable-v2.xml" \
	tablet-unstable-v2-protocol.c
"$SCANNER" client-header "$PROTO/stable/xdg-shell/xdg-shell.xml" \
	xdg-shell-client-protocol.h
"$SCANNER" private-code  "$PROTO/stable/xdg-shell/xdg-shell.xml" \
	xdg-shell-protocol.c
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

PKGCFG="fcft pixman-1 xkbcommon wayland-client"

gcc $CFLAGS -O2 -std=gnu11 -D_GNU_SOURCE -Wall -Wextra \
	-I. -I"$PORT_SRC" \
	-I"$LIBS/libkbase" -I"$LIBS/libktui" -I"$LIBS/libkcolor" -I"$LIBS/libkcell" -I"$LIBS/libkwl" \
	$(pkg-config --cflags $PKGCFG) \
	-o kdos-lock \
	"$PORT_SRC"/main.c \
	"$LIBS"/libkwl/*.c "$LIBS"/libkcell/*.c "$LIBS"/libktui/*.c "$LIBS"/libkcolor/*.c \
	"$LIBS"/libkbase/*.c \
	./*-protocol.c \
	$(pkg-config --libs $PKGCFG) $LDFLAGS

gcc $CFLAGS -O2 -std=gnu11 -D_GNU_SOURCE -Wall -Wextra \
	-o kdos-checkpass "$PORT_SRC"/checkpass.c -lcrypt $LDFLAGS

install -Dm755 kdos-lock "$PKG/usr/bin/kdos-lock"
# 4755, and this is the only setuid bit KDOS ships. /etc/shadow is root-only
# and the lock screen must not be root; see checkpass.c's header for what the
# 120 lines behind this bit are allowed to do.
install -Dm4755 kdos-checkpass "$PKG/usr/bin/kdos-checkpass"

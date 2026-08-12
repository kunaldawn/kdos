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

# source= is empty: this is ours. It compiles out of $PORT_SRC together with
# libkwl and libktui, which are static archives in the tree rather than
# installed libraries.
LIBS="$PORT_SRC/../../libs"

SCANNER="$(pkg-config --variable=wayland_scanner wayland-scanner)"
PROTO="$(pkg-config --variable=pkgdatadir wayland-protocols)"

# Three protocol definitions from three places, which is the usual state of
# Wayland: xdg-shell is stable and in wayland-protocols, ext-workspace is
# staging and also there, and layer-shell and foreign-toplevel are wlroots'
# own — installed by the wlroots port precisely because upstream does not.
"$SCANNER" client-header "$PROTO/stable/xdg-shell/xdg-shell.xml" \
	xdg-shell-client-protocol.h
"$SCANNER" private-code  "$PROTO/stable/xdg-shell/xdg-shell.xml" \
	xdg-shell-protocol.c
"$SCANNER" client-header "$PROTO/staging/ext-workspace/ext-workspace-v1.xml" \
	ext-workspace-v1-client-protocol.h
"$SCANNER" private-code  "$PROTO/staging/ext-workspace/ext-workspace-v1.xml" \
	ext-workspace-v1-protocol.c
for p in wlr-layer-shell-unstable-v1 wlr-foreign-toplevel-management-unstable-v1; do
	"$SCANNER" client-header "/usr/share/wlroots/protocols/$p.xml" \
		"$p-client-protocol.h"
	"$SCANNER" private-code  "/usr/share/wlroots/protocols/$p.xml" \
		"$p-protocol.c"
done

PKGCFG="fcft pixman-1 xkbcommon wayland-client basu alsa libpipewire-0.3"

gcc $CFLAGS -O2 -std=gnu11 -D_GNU_SOURCE -Wall -Wextra \
	-I. -I"$PORT_SRC" \
	-I"$LIBS/libkbase" -I"$LIBS/libktui" -I"$LIBS/libkcolor" -I"$LIBS/libkwl" \
	-I"$LIBS/libkxdg" \
	$(pkg-config --cflags $PKGCFG) \
	-o kdos-shell \
	"$PORT_SRC"/*.c \
	"$LIBS"/libkwl/*.c "$LIBS"/libktui/*.c "$LIBS"/libkcolor/*.c \
	"$LIBS"/libkbase/*.c "$LIBS"/libkxdg/*.c \
	./*-protocol.c \
	$(pkg-config --libs $PKGCFG) $LDFLAGS

install -Dm755 kdos-shell "$PKG/usr/bin/kdos-shell"
# Dispatched on its own basename, so the launcher is a link rather than a
# second binary. `bind Super+D = spawn kdos-launcher` in comp.conf reaches it.
ln -s kdos-shell "$PKG/usr/bin/kdos-launcher"
ln -s kdos-shell "$PKG/usr/bin/kdos-notifyd"
ln -s kdos-shell "$PKG/usr/bin/kdos-osd"

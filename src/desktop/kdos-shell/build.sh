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

# Four protocol definitions from three places, which is the usual state of
# Wayland: xdg-shell is stable and in wayland-protocols, ext-workspace and
# ext-session-lock are staging and also there, and layer-shell and
# foreign-toplevel are wlroots' own — installed by the wlroots port precisely
# because upstream does not.
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
"$SCANNER" client-header "$PROTO/staging/ext-workspace/ext-workspace-v1.xml" \
	ext-workspace-v1-client-protocol.h
"$SCANNER" private-code  "$PROTO/staging/ext-workspace/ext-workspace-v1.xml" \
	ext-workspace-v1-protocol.c
# The panel is not a lock screen and binds no lock role — but it compiles the
# WHOLE of libkwl, and libkwl carries KWL_ROLE_LOCK for kdos-lock, so kwl.c
# includes this header unconditionally. Generating it only in kdos-lock's
# recipe is what made kdos-shell the first package of this phase to fail.
"$SCANNER" client-header \
	"$PROTO/staging/ext-session-lock/ext-session-lock-v1.xml" \
	ext-session-lock-v1-client-protocol.h
"$SCANNER" private-code \
	"$PROTO/staging/ext-session-lock/ext-session-lock-v1.xml" \
	ext-session-lock-v1-protocol.c
# wlr-output-management is kdos-display's: labwc takes its screen configuration
# over it like every wlroots compositor, and nothing in this tree spoke it, so
# there was no way at all to set a mode, a scale or a second monitor's place.
for p in wlr-layer-shell-unstable-v1 wlr-foreign-toplevel-management-unstable-v1 \
	 wlr-output-management-unstable-v1; do
	"$SCANNER" client-header "/usr/share/wlroots/protocols/$p.xml" \
		"$p-client-protocol.h"
	"$SCANNER" private-code  "/usr/share/wlroots/protocols/$p.xml" \
		"$p-protocol.c"
done

PKGCFG="fcft pixman-1 xkbcommon wayland-client basu alsa libpipewire-0.3"

gcc $CFLAGS -O2 -std=gnu11 -D_GNU_SOURCE -Wall -Wextra \
	-I. -I"$PORT_SRC" \
	-I"$LIBS/libkbase" -I"$LIBS/libktui" -I"$LIBS/libkcolor" -I"$LIBS/libkcell" -I"$LIBS/libkwl" \
	-I"$LIBS/libkxdg" \
	$(pkg-config --cflags $PKGCFG) \
	-o kdos-shell \
	"$PORT_SRC"/*.c \
	"$LIBS"/libkwl/*.c "$LIBS"/libkcell/*.c "$LIBS"/libktui/*.c "$LIBS"/libkcolor/*.c \
	"$LIBS"/libkbase/*.c "$LIBS"/libkxdg/*.c \
	./*-protocol.c \
	$(pkg-config --libs $PKGCFG) $LDFLAGS

install -Dm755 kdos-shell "$PKG/usr/bin/kdos-shell"
# Dispatched on its own basename, so the launcher is a link rather than a
# second binary. The skel rc.xml's W-d keybind Executes it by that name.
ln -s kdos-shell "$PKG/usr/bin/kdos-launcher"
ln -s kdos-shell "$PKG/usr/bin/kdos-menu"
ln -s kdos-shell "$PKG/usr/bin/kdos-desk"
ln -s kdos-shell "$PKG/usr/bin/kdos-pick"
ln -s kdos-shell "$PKG/usr/bin/kdos-ascii"
ln -s kdos-shell "$PKG/usr/bin/kdos-run"
# kdos-comp's <core><promptCommand> — the yes/no dialog labwc's If/prompt
# action needs. Upstream's labnag is not built (-Dlabnag=disabled).
ln -s kdos-shell "$PKG/usr/bin/kdos-prompt"
ln -s kdos-shell "$PKG/usr/bin/kdos-notifyd"
ln -s kdos-shell "$PKG/usr/bin/kdos-osd"
# The clock's other half, and the screens. The panel spawns kdos-cal by name.
ln -s kdos-shell "$PKG/usr/bin/kdos-cal"
ln -s kdos-shell "$PKG/usr/bin/kdos-display"

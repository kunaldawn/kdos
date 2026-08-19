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
# primary-selection: middle-click paste is a SECOND selection with its own
# device manager, and libkwl includes the header unconditionally. The
# private-code is what carries the zwp_primary_selection_* interface symbols
# the link needs; generating only the header builds and then fails at link.
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
# wlr-data-control is kdos-clip's, and it is the only protocol that can carry a
# clipboard HISTORY: wl_data_device delivers a selection event solely to the
# client with keyboard focus, so a manager built on it records nothing.
for p in wlr-layer-shell-unstable-v1 wlr-foreign-toplevel-management-unstable-v1 \
	 wlr-output-management-unstable-v1 wlr-data-control-unstable-v1; do
	"$SCANNER" client-header "/usr/share/wlroots/protocols/$p.xml" \
		"$p-client-protocol.h"
	"$SCANNER" private-code  "/usr/share/wlroots/protocols/$p.xml" \
		"$p-protocol.c"
done

# libpng is libkicon's: the icon layer decodes the alien apps' own PNGs out of
# /usr/share/icons/hicolor and the theme's atlas, and there is no SVG parser
# anywhere in this tree.
PKGCFG="fcft pixman-1 xkbcommon wayland-client basu alsa libpipewire-0.3 libpng"

gcc $CFLAGS -O2 -std=gnu11 -D_GNU_SOURCE -Wall -Wextra \
	-I. -I"$PORT_SRC" \
	-I"$LIBS/libkbase" -I"$LIBS/libktui" -I"$LIBS/libkcolor" -I"$LIBS/libkcell" -I"$LIBS/libkwl" \
	-I"$LIBS/libkxdg" -I"$LIBS/libkicon" -I"$LIBS/libkchrome" \
	-I"$LIBS/libkproc" \
	$(pkg-config --cflags $PKGCFG) \
	-o kdos-shell \
	"$PORT_SRC"/*.c \
	"$LIBS"/libkwl/*.c "$LIBS"/libkcell/*.c "$LIBS"/libktui/*.c "$LIBS"/libkcolor/*.c \
	"$LIBS"/libkbase/*.c "$LIBS"/libkxdg/*.c "$LIBS"/libkicon/*.c \
	"$LIBS"/libkchrome/*.c "$LIBS"/libkproc/*.c \
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
# The notification CENTRE: the daemon keeps a history and this draws it. A
# notification that expired is not a notification that was read, and on a
# desktop whose fat applications all live in containers a boxed app's toast is
# often the only thing that says its work has finished.
ln -s kdos-shell "$PKG/usr/bin/kdos-notify"
ln -s kdos-shell "$PKG/usr/bin/kdos-osd"
# The clock's other half, and the screens. The panel spawns kdos-cal by name.
ln -s kdos-shell "$PKG/usr/bin/kdos-cal"
ln -s kdos-shell "$PKG/usr/bin/kdos-display"
# The keybind card is generated from the same rc.xml the compositor reads, so
# help cannot drift from what the keys actually do; W-F1 opens it.
ln -s kdos-shell "$PKG/usr/bin/kdos-keys"
ln -s kdos-shell "$PKG/usr/bin/kdos-teams"
ln -s kdos-shell "$PKG/usr/bin/kdos-saver"
ln -s kdos-shell "$PKG/usr/bin/kdos-slit"
ln -s kdos-shell "$PKG/usr/bin/kdos-doc"
ln -s kdos-shell "$PKG/usr/bin/kdos-settings"
ln -s kdos-shell "$PKG/usr/bin/kdos-openwith"
ln -s kdos-shell "$PKG/usr/bin/kdos-audio"
# The Start menu — the panel's own button, and the one front door this desktop
# did not have. kdos-menu and kdos-launcher keep the jobs they are good at.
ln -s kdos-shell "$PKG/usr/bin/kdos-start"
# The device managers. Everything they talk to (NetworkManager, bluez, V4L2)
# has been running here since before there was a desktop; what was missing was
# a surface, and `foot -e nmtui` was it.
ln -s kdos-shell "$PKG/usr/bin/kdos-net"
ln -s kdos-shell "$PKG/usr/bin/kdos-bt"
ln -s kdos-shell "$PKG/usr/bin/kdos-devices"
# The clipboard history: the daemon the compositor supervises, and the picker
# `W-v` opens. One binary, one name, two roles.
ln -s kdos-shell "$PKG/usr/bin/kdos-clip"
# The notification area's overflow: the occasional widgets live behind one
# chevron of fixed width so the right wing stops changing size, and this is
# what the chevron opens. It also runs `kdos stutter` and `kdos-energy` inside
# a pane, which is what those two had instead of a surface.
ln -s kdos-shell "$PKG/usr/bin/kdos-status"
# The tooltip. Half the bar is 32-pixel pictures with no words beside them, and
# a control nobody can name is a control nobody clicks.
ln -s kdos-shell "$PKG/usr/bin/kdos-tip"

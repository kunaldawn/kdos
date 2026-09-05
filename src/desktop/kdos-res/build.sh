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

# ONE BINARY, TWO FACES, and a setuid helper built separately in Task 9's
# shape: kdos-res links libkwl and half of Wayland, and anything setuid must
# not.
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
"$SCANNER" client-header \
	/usr/share/wlroots/protocols/wlr-foreign-toplevel-management-unstable-v1.xml \
	wlr-foreign-toplevel-management-unstable-v1-client-protocol.h
"$SCANNER" private-code \
	/usr/share/wlroots/protocols/wlr-foreign-toplevel-management-unstable-v1.xml \
	wlr-foreign-toplevel-management-unstable-v1-protocol.c

# libpng is libkicon's: the icon layer decodes the alien apps' own PNGs.
PKGCFG="fcft pixman-1 xkbcommon wayland-client libpng"

# EVERY .c IN THE PORT EXCEPT THE HELPER, by glob rather than by a list. The
# list was a second place to remember a new file, and the selftest — which
# globs — built a source the shipped recipe did not, so a whole page could
# pass every gate on this machine and fail to LINK in the build.
# resctl.c is the one exclusion: it is setuid and is built on its own line
# below, linking libkbase and nothing else.
RES_SRC=""
for f in "$PORT_SRC"/*.c; do
	case "$f" in
	*/resctl.c) continue ;;
	esac
	RES_SRC="$RES_SRC $f"
done

gcc $CFLAGS -O2 -std=gnu11 -D_GNU_SOURCE -Wall -Wextra \
	-DKDOS_RES_VERSION="\"$version\"" \
	-I. -I"$PORT_SRC" \
	-I"$LIBS/libkbase" -I"$LIBS/libktui" -I"$LIBS/libkcolor" \
	-I"$LIBS/libkcell" -I"$LIBS/libkwl" -I"$LIBS/libkdisp" -I"$LIBS/libkcon" -I"$LIBS/libkxdg" \
	-I"$LIBS/libkicon" -I"$LIBS/libkchrome" -I"$LIBS/libkproc" \
	$(pkg-config --cflags $PKGCFG) \
	-o kdos-res \
	$RES_SRC \
	"$LIBS"/libkwl/*.c "$LIBS"/libkdisp/*.c "$LIBS"/libkcon/*.c "$LIBS"/libkcell/*.c "$LIBS"/libktui/*.c \
	"$LIBS"/libkcolor/*.c "$LIBS"/libkbase/*.c "$LIBS"/libkxdg/*.c \
	"$LIBS"/libkicon/*.c "$LIBS"/libkchrome/*.c "$LIBS"/libkproc/*.c \
	./*-protocol.c \
	$(pkg-config --libs $PKGCFG) -ldl $LDFLAGS

# The setuid helper is built SEPARATELY and links libkbase and nothing else.
# Giving a setuid binary the Wayland stack would be handing root a font parser;
# see resctl.c's header for the whole of what these three verbs may do.
gcc $CFLAGS -O2 -std=gnu11 -D_GNU_SOURCE -Wall -Wextra \
	-I"$LIBS/libkbase" \
	-o kdos-resctl "$PORT_SRC"/resctl.c "$LIBS"/libkbase/*.c $LDFLAGS

install -Dm755 kdos-res "$PKG/usr/bin/kdos-res"
# 4755, and this is the SECOND setuid bit KDOS ships. The first is
# kdos-checkpass, which exists because /etc/shadow is root-only; this one
# exists because a task manager that cannot end a stuck root daemon and a
# Memory page that cannot read SMBIOS are each half a feature.
install -Dm4755 kdos-resctl "$PKG/usr/bin/kdos-resctl"
install -Dm644 "$PORT_SRC/kdos-res.desktop" \
	"$PKG/usr/share/applications/kdos-res.desktop"

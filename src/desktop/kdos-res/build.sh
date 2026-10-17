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

PKGCFG="fcft fontconfig pixman-1 xkbcommon wayland-client libpng"

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
	-I"$LIBS/libkcell" -I"$LIBS/libkwl" -I"$LIBS/libkdisp" -I"$LIBS/libkxdg" \
	-I"$LIBS/libkicon" -I"$LIBS/libkchrome" -I"$LIBS/libkproc" \
	$(pkg-config --cflags $PKGCFG) \
	-o kdos-res \
	$RES_SRC \
	"$LIBS"/libkwl/*.c "$LIBS"/libkdisp/*.c "$LIBS"/libkcell/*.c "$LIBS"/libktui/*.c \
	"$LIBS"/libkcolor/*.c "$LIBS"/libkbase/*.c "$LIBS"/libkxdg/*.c \
	"$LIBS"/libkicon/*.c "$LIBS"/libkchrome/*.c "$LIBS"/libkproc/*.c \
	./*-protocol.c \
	$(pkg-config --libs $PKGCFG) -ldl $LDFLAGS

gcc $CFLAGS -O2 -std=gnu11 -D_GNU_SOURCE -Wall -Wextra \
	-I"$LIBS/libkbase" \
	-o kdos-resctl "$PORT_SRC"/resctl.c "$LIBS"/libkbase/*.c $LDFLAGS

install -Dm755 kdos-res "$PKG/usr/bin/kdos-res"
install -Dm4755 kdos-resctl "$PKG/usr/bin/kdos-resctl"
install -Dm644 "$PORT_SRC/kdos-res.desktop" \
	"$PKG/usr/share/applications/kdos-res.desktop"

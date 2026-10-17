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
"$SCANNER" client-header "$PROTO/staging/ext-workspace/ext-workspace-v1.xml" \
	ext-workspace-v1-client-protocol.h
"$SCANNER" private-code  "$PROTO/staging/ext-workspace/ext-workspace-v1.xml" \
	ext-workspace-v1-protocol.c
"$SCANNER" client-header \
	"$PROTO/staging/ext-session-lock/ext-session-lock-v1.xml" \
	ext-session-lock-v1-client-protocol.h
"$SCANNER" private-code \
	"$PROTO/staging/ext-session-lock/ext-session-lock-v1.xml" \
	ext-session-lock-v1-protocol.c
for p in wlr-layer-shell-unstable-v1 wlr-foreign-toplevel-management-unstable-v1 \
	 wlr-output-management-unstable-v1 wlr-data-control-unstable-v1; do
	"$SCANNER" client-header "/usr/share/wlroots/protocols/$p.xml" \
		"$p-client-protocol.h"
	"$SCANNER" private-code  "/usr/share/wlroots/protocols/$p.xml" \
		"$p-protocol.c"
done

PKGCFG="fcft fontconfig pixman-1 xkbcommon wayland-client basu alsa libpipewire-0.3 libpng libjpeg libwebp libnsgif libarchive"

gcc $CFLAGS -O2 -std=gnu11 -D_GNU_SOURCE -Wall -Wextra \
	-DKIMG_HAVE_PNG -DKIMG_HAVE_JPEG -DKIMG_HAVE_WEBP -DKIMG_HAVE_GIF \
	-I. -I"$PORT_SRC" \
	-I"$LIBS/libkbase" -I"$LIBS/libktui" -I"$LIBS/libkcolor" -I"$LIBS/libkcell" -I"$LIBS/libkwl" -I"$LIBS/libkdisp" -I"$LIBS/libkwm" \
	-I"$LIBS/libkxdg" -I"$LIBS/libkicon" -I"$LIBS/libkchrome" \
	-I"$LIBS/libkimg" \
	-I"$LIBS/libkproc" -I"$LIBS/libksig" \
	$(pkg-config --cflags $PKGCFG) \
	-o kdos-shell \
	"$PORT_SRC"/*.c \
	"$LIBS"/libkwl/*.c "$LIBS"/libkdisp/*.c "$LIBS"/libkwm/*.c "$LIBS"/libkcell/*.c "$LIBS"/libktui/*.c "$LIBS"/libkcolor/*.c \
	"$LIBS"/libkbase/*.c "$LIBS"/libkxdg/*.c "$LIBS"/libkicon/*.c \
	"$LIBS"/libkchrome/*.c "$LIBS"/libkproc/*.c "$LIBS"/libkimg/*.c \
	"$LIBS"/libksig/*.c "$LIBS"/libksig/monocypher/*.c \
	./*-protocol.c \
	$(pkg-config --libs $PKGCFG) $LDFLAGS

gcc $CFLAGS -O2 -std=gnu11 -D_GNU_SOURCE -Wall -Wextra \
	-I"$PORT_SRC" -o mkcharidx "$PORT_SRC/tools/mkcharidx.c" \
	$(pkg-config --cflags --libs icu-uc) $LDFLAGS
./mkcharidx charnames.idx
install -Dm644 charnames.idx "$PKG/usr/share/kdos/charnames.idx"

install -Dm755 kdos-shell "$PKG/usr/bin/kdos-shell"
ln -s kdos-shell "$PKG/usr/bin/kdos-mediad"
ln -s kdos-shell "$PKG/usr/bin/kdos-launcher"
ln -s kdos-shell "$PKG/usr/bin/kdos-menu"
ln -s kdos-shell "$PKG/usr/bin/kdos-desk"
ln -s kdos-shell "$PKG/usr/bin/kdos-pick"
ln -s kdos-shell "$PKG/usr/bin/kdos-peek"
ln -s kdos-shell "$PKG/usr/bin/kdos-find"
ln -s kdos-shell "$PKG/usr/bin/kdos-pix"
ln -s kdos-shell "$PKG/usr/bin/kdos-rec"
ln -s kdos-shell "$PKG/usr/bin/kdos-ascii"
ln -s kdos-shell "$PKG/usr/bin/kdos-palette"
ln -s kdos-shell "$PKG/usr/bin/kdos-about"
ln -s kdos-shell "$PKG/usr/bin/kdos-style"
ln -s kdos-shell "$PKG/usr/bin/kdos-calc"
ln -s kdos-shell "$PKG/usr/bin/kdos-chars"
ln -s kdos-shell "$PKG/usr/bin/kdos-connect"
ln -s kdos-shell "$PKG/usr/bin/kdos-traymenu"
ln -s kdos-shell "$PKG/usr/bin/kdos-contacts"
ln -s kdos-shell "$PKG/usr/bin/kdos-disks"
ln -s kdos-shell "$PKG/usr/bin/kdos-print"
ln -s kdos-shell "$PKG/usr/bin/kdos-time"
ln -s kdos-shell "$PKG/usr/bin/kdos-users"
ln -s kdos-shell "$PKG/usr/bin/kdos-update"
ln -s kdos-shell "$PKG/usr/bin/kdos-store"
ln -s kdos-shell "$PKG/usr/bin/kdos-firewall"
ln -s kdos-shell "$PKG/usr/bin/kdos-backup"
ln -s kdos-shell "$PKG/usr/bin/kdos-burn"
ln -s kdos-shell "$PKG/usr/bin/kdos-verify"
ln -s kdos-shell "$PKG/usr/bin/kdos-note"
ln -s kdos-shell "$PKG/usr/bin/kdos-run"
ln -s kdos-shell "$PKG/usr/bin/kdos-prompt"
ln -s kdos-shell "$PKG/usr/bin/kdos-notifyd"
ln -s kdos-shell "$PKG/usr/bin/kdos-netagent"
ln -s kdos-shell "$PKG/usr/bin/kdos-notify"
ln -s kdos-shell "$PKG/usr/bin/kdos-osd"
ln -s kdos-shell "$PKG/usr/bin/kdos-cal"
ln -s kdos-shell "$PKG/usr/bin/kdos-display"
ln -s kdos-shell "$PKG/usr/bin/kdos-keys"
ln -s kdos-shell "$PKG/usr/bin/kdos-teams"
ln -s kdos-shell "$PKG/usr/bin/kdos-saver"
ln -s kdos-shell "$PKG/usr/bin/kdos-slit"
ln -s kdos-shell "$PKG/usr/bin/kdos-doc"
ln -s kdos-shell "$PKG/usr/bin/kdos-settings"
ln -s kdos-shell "$PKG/usr/bin/kdos-openwith"
ln -s kdos-shell "$PKG/usr/bin/kdos-audio"
ln -s kdos-shell "$PKG/usr/bin/kdos-start"
ln -s kdos-shell "$PKG/usr/bin/kdos-net"
ln -s kdos-shell "$PKG/usr/bin/kdos-bt"
ln -s kdos-shell "$PKG/usr/bin/kdos-devices"
ln -s kdos-shell "$PKG/usr/bin/kdos-clip"
ln -s kdos-shell "$PKG/usr/bin/kdos-trash"
ln -s kdos-shell "$PKG/usr/bin/kdos-status"
ln -s kdos-shell "$PKG/usr/bin/kdos-tip"
ln -s kdos-shell "$PKG/usr/bin/kdos-ime"

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

gcc $CFLAGS -O2 -std=gnu11 -D_GNU_SOURCE -Wall -Wextra \
	-I"$LIBS/libkbase" -I"$LIBS/libkxdg" $(pkg-config --cflags basu) \
	-o kdos-notice "$PORT_SRC"/*.c "$LIBS"/libkbase/*.c "$LIBS"/libkxdg/*.c \
	$(pkg-config --libs basu) $LDFLAGS

install -Dm755 kdos-notice "$PKG/usr/bin/kdos-notice"
install -Dm644 "$PORT_SRC/kdos-notice.desktop" \
	"$PKG/usr/share/kdos/xdg/autostart/kdos-notice.desktop"

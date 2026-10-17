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

XML="$(pkg-config --variable=pkgdatadir wayland-protocols)/staging/security-context/security-context-v1.xml"
SCANNER="$(pkg-config --variable=wayland_scanner wayland-scanner)"

"$SCANNER" client-header "$XML" security-context-v1-client-protocol.h
"$SCANNER" private-code  "$XML" security-context-v1-protocol.c

gcc $CFLAGS -O2 -Wall -Wextra \
	-I. \
	$(pkg-config --cflags wayland-client) \
	-o kdos-boxsock "$PORT_SRC"/main.c security-context-v1-protocol.c \
	$(pkg-config --libs wayland-client) $LDFLAGS

install -Dm755 kdos-boxsock "$PKG/usr/bin/kdos-boxsock"

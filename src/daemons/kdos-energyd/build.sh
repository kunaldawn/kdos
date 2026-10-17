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
	-I"$LIBS/libkbase" -I"$LIBS/libkproc" -I"$PORT_SRC" \
	-o kdos-energyd "$PORT_SRC"/*.c "$LIBS"/libkbase/*.c "$LIBS"/libkproc/*.c $LDFLAGS

install -Dm755 kdos-energyd "$PKG/usr/sbin/kdos-energyd"
install -d "$PKG/usr/bin"
ln -s ../sbin/kdos-energyd "$PKG/usr/bin/kdos-energy"

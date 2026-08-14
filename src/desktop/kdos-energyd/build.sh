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

# One binary, two names, dispatched on its own basename. libkbase and nothing
# else: the daemon reads a root-only counter, and every library it links is code
# running as root.
LIBS="$PORT_SRC/../../libs"

gcc $CFLAGS -O2 -std=gnu11 -D_GNU_SOURCE -Wall -Wextra \
	-I"$LIBS/libkbase" -I"$PORT_SRC" \
	-o kdos-energyd "$PORT_SRC"/*.c "$LIBS"/libkbase/*.c $LDFLAGS

install -Dm755 kdos-energyd "$PKG/usr/sbin/kdos-energyd"
install -d "$PKG/usr/bin"
ln -s ../sbin/kdos-energyd "$PKG/usr/bin/kdos-energy"

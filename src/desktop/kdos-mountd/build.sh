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

# One binary, two names, dispatched on its own basename — kdos-powerd's shape.
# libkbase and nothing else: this is a root daemon, and every library it links
# is code running as root.
LIBS="$PORT_SRC/../../libs"

gcc $CFLAGS -O2 -std=gnu11 -D_GNU_SOURCE -Wall -Wextra \
	-I"$LIBS/libkbase" \
	-o kdos-mountd "$PORT_SRC"/main.c "$LIBS"/libkbase/*.c $LDFLAGS

install -Dm755 kdos-mountd "$PKG/usr/sbin/kdos-mountd"
install -d "$PKG/usr/bin"
ln -s ../sbin/kdos-mountd "$PKG/usr/bin/kdos-mount"

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

# STATIC, and that is the whole reason this program can exist. It runs inside a
# box whose /usr comes from a Debian pack; a dynamically linked host binary
# bind-mounted in would look for musl there and find glibc.
LIBS="$PORT_SRC/../../libs"

gcc $CFLAGS -O2 -std=gnu11 -D_GNU_SOURCE -Wall -Wextra -static \
	-I"$LIBS/libkbase" \
	-o kdos-boxinit "$PORT_SRC"/main.c "$LIBS"/libkbase/*.c $LDFLAGS

install -Dm755 kdos-boxinit "$PKG/usr/libexec/kdos/kdos-boxinit"

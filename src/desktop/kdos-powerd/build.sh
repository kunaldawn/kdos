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

# One binary, two names, dispatched on its own basename. libkbase and libkcolor
# and nothing else: this is a root daemon, and every library it links is code
# running as root.
#
# libkcolor is here so the `accent` verb can refuse a name that is not one of
# the seven compiled-in schemes. That is the WHOLE authorisation argument for
# reaching this verb from an unprivileged session — the argument is matched
# against a closed list rather than sanitised — so the check has to be the
# palette's own and not a character class copied into this file. The library
# links nothing itself, allocates nothing from its input on the path used here,
# and its arithmetic is integer and table-driven.
LIBS="$PORT_SRC/../../libs"

gcc $CFLAGS -O2 -std=gnu11 -D_GNU_SOURCE -Wall -Wextra \
	-I"$LIBS/libkbase" -I"$LIBS/libkcolor" \
	-o kdos-powerd "$PORT_SRC"/main.c \
	"$LIBS"/libkbase/*.c "$LIBS"/libkcolor/*.c $LDFLAGS

install -Dm755 kdos-powerd "$PKG/usr/sbin/kdos-powerd"
install -d "$PKG/usr/bin"
ln -s ../sbin/kdos-powerd "$PKG/usr/bin/kdos-power"

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

# libkbase, libksig, libkpack and libkpkg — every library a root daemon links
# is code running as root, which is why libkpack was written to link nothing
# more than these.
LIBS="$PORT_SRC/../../libs"

gcc $CFLAGS -O2 -std=gnu11 -D_GNU_SOURCE -Wall -Wextra \
	-I"$LIBS/libkbase" -I"$LIBS/libksig" -I"$LIBS/libkpkg" \
	-I"$LIBS/libkpack" -I"$PORT_SRC" \
	-o kdos-packd "$PORT_SRC"/*.c \
	"$LIBS"/libkbase/*.c "$LIBS"/libksig/*.c \
	"$LIBS"/libksig/monocypher/*.c "$LIBS"/libkpkg/*.c \
	"$LIBS"/libkpack/*.c $LDFLAGS

install -Dm755 kdos-packd "$PKG/usr/sbin/kdos-packd"
install -d "$PKG/var/lib/kdos/packs/staging" "$PKG/var/lib/kdos/packs/mnt"

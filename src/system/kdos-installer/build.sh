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
CORE="$PORT_SRC/../kinstall-core"
APPBOX="$PORT_SRC/../kdos-appbox"

mkdir -p core
for f in "$CORE"/*.c "$APPBOX"/catalogue.c; do
	gcc $CFLAGS -O2 -std=gnu11 -D_GNU_SOURCE -Wall -Wextra \
		-I"$LIBS/libkbase" -I"$LIBS/libkcolor" -I"$CORE" -I"$APPBOX" \
		-c -o "core/$(basename "$f" .c).o" "$f"
done
ar rcs libkinstall-core.a core/*.o

gcc $CFLAGS -O2 -std=gnu11 -D_GNU_SOURCE -Wall -Wextra \
	-I"$LIBS/libkbase" -I"$LIBS/libktui" -I"$LIBS/libkcolor" \
	-I"$CORE" -I"$PORT_SRC" \
	-o kinstall \
	"$PORT_SRC"/*.c libkinstall-core.a \
	"$LIBS"/libkbase/*.c "$LIBS"/libktui/*.c "$LIBS"/libkcolor/*.c $LDFLAGS

install -Dm755 kinstall "$PKG/usr/bin/kinstall"

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

# Built in the work dir, never in $PORT_SRC — that is the repo.
LIBS="$PORT_SRC/../../libs"

gcc $CFLAGS -O2 -std=c11 -D_GNU_SOURCE -Wall -Wextra \
	-I"$LIBS/libkbase" -I"$LIBS/libktui" -I"$LIBS/libkcolor" \
	-I"$LIBS/libkxdg" -I"$PORT_SRC" -o kdos-appbox \
	"$PORT_SRC/main.c" "$PORT_SRC/util.c" "$PORT_SRC/box.c" \
	"$PORT_SRC/app.c" "$PORT_SRC/tui.c" "$PORT_SRC/launchers.c" \
	"$PORT_SRC/image.c" "$PORT_SRC/open.c" "$PORT_SRC/pack.c" \
	"$PORT_SRC/box_cmd.c" \
	"$LIBS"/libkbase/*.c "$LIBS"/libktui/*.c "$LIBS"/libkcolor/*.c \
	"$LIBS"/libkxdg/*.c $LDFLAGS
install -Dm755 kdos-appbox "$PKG/usr/local/bin/kdos-appbox"
# A name the build does not symlink is a program nothing can reach.
ln -s kdos-appbox "$PKG/usr/local/bin/kdos-box"

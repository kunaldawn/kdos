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
	-I"$LIBS/libkbase" -I"$LIBS/libkxdg" -I"$LIBS/libkcolor" \
	$(pkg-config --cflags basu sqlite3) \
	-o kdos-runner "$PORT_SRC"/*.c \
	"$LIBS"/libkbase/*.c "$LIBS"/libkxdg/*.c "$LIBS"/libkcolor/*.c \
	$(pkg-config --libs basu sqlite3) $LDFLAGS

install -Dm755 kdos-runner "$PKG/usr/libexec/kdos/kdos-runner"
install -Dm644 "$PORT_SRC/io.kdos.Runner.service" \
	"$PKG/usr/share/dbus-1/services/io.kdos.Runner.service"
for _p in kdos kdos-library; do
	install -Dm644 "$PORT_SRC/$_p.desktop" \
		"$PKG/usr/share/krunner/dbusplugins/$_p.desktop"
done

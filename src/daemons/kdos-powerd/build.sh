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
	-I"$LIBS/libkbase" -I"$LIBS/libkcolor" $(pkg-config --cflags basu) \
	-o kdos-powerd "$PORT_SRC"/main.c "$PORT_SRC"/bus.c \
	"$LIBS"/libkbase/*.c "$LIBS"/libkcolor/*.c \
	$(pkg-config --libs basu) $LDFLAGS

install -Dm755 kdos-powerd "$PKG/usr/sbin/kdos-powerd"
install -d "$PKG/usr/bin"
ln -s ../sbin/kdos-powerd "$PKG/usr/bin/kdos-power"
install -Dm644 "$PORT_SRC/io.kdos.Power1.conf" \
	"$PKG/usr/share/dbus-1/system.d/io.kdos.Power1.conf"
install -Dm644 "$PORT_SRC/io.kdos.power1.policy" \
	"$PKG/usr/share/polkit-1/actions/io.kdos.power1.policy"

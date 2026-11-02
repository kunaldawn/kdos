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
	$(pkg-config --cflags basu) -I"$LIBS/libkbase" \
	-o kdos-sysupdated "$PORT_SRC"/*.c "$LIBS"/libkbase/*.c \
	$(pkg-config --libs basu) $LDFLAGS

install -Dm755 kdos-sysupdated "$PKG/usr/sbin/kdos-sysupdated"
install -Dm644 "$PORT_SRC/kdos-sysupdated.conf" \
	"$PKG/usr/share/dbus-1/system.d/kdos-sysupdated.conf"
install -Dm644 "$PORT_SRC/org.freedesktop.sysupdate1.policy" \
	"$PKG/usr/share/polkit-1/actions/org.freedesktop.sysupdate1.policy"
install -Dm644 "$PORT_SRC/io.kdos.slots.policy" \
	"$PKG/usr/share/polkit-1/actions/io.kdos.slots.policy"
install -Dm644 "$PORT_SRC/io.kdos.system.metainfo.xml" \
	"$PKG/usr/share/kdos/update/io.kdos.system.metainfo.xml"

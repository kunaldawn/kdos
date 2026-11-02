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
	-o kdos-settingsd "$PORT_SRC"/*.c "$LIBS"/libkbase/*.c \
	$(pkg-config --libs basu) $LDFLAGS

install -Dm755 kdos-settingsd "$PKG/usr/sbin/kdos-settingsd"
install -Dm644 "$PORT_SRC/kdos-settingsd.conf" \
	"$PKG/usr/share/dbus-1/system.d/kdos-settingsd.conf"
for _i in hostname1 timedate1 locale1; do
	install -Dm644 "$PORT_SRC/org.freedesktop.$_i.policy" \
		"$PKG/usr/share/polkit-1/actions/org.freedesktop.$_i.policy"
done

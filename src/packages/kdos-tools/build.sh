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
	-I"$LIBS/libkbase" -I"$LIBS/libkcolor" -I"$PORT_SRC" \
	-o kdos-tools \
	"$PORT_SRC"/*.c \
	"$LIBS"/libkbase/*.c "$LIBS"/libkcolor/*.c $LDFLAGS

install -Dm755 kdos-tools "$PKG/usr/sbin/ksvc"

# Dispatched on its own basename, so every name is a link to the one
# binary. /etc/inittab wraps both gettys in kdos-getty and every
# init.d script reaches ksvc through service_helper.
ln -s ksvc "$PKG/usr/sbin/service"
install -d "$PKG/usr/local/sbin"
ln -s /usr/sbin/ksvc "$PKG/usr/local/sbin/kdos-getty"

install -d "$PKG/usr/local/bin"
for t in kdos kdos-banner kdos-shot kdos-fetch-app kdos-fetch-static; do
	ln -s /usr/sbin/ksvc "$PKG/usr/local/bin/$t"
done

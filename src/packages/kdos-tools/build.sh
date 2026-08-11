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
	-I"$LIBS/libkbase" -I"$LIBS/libkcolor" -I"$LIBS/libkpkg" -I"$PORT_SRC" \
	-o kdos-tools \
	"$PORT_SRC"/*.c \
	"$LIBS"/libkbase/*.c "$LIBS"/libkcolor/*.c "$LIBS"/libkpkg/*.c $LDFLAGS

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

# The recorded debug cycles, queryable offline by `kdos why` and
# `kdos explain`. Ships in the ISO like the appbox does: a help system that
# needs a network is no help on the machine that will not boot.
install -d "$PKG/usr/share/kdos/reasons"
install -m644 "$PORT_SRC"/../../../reasons/*.txt "$PKG/usr/share/kdos/reasons/"

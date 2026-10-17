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
mkdir -p klibs
(cd klibs && gcc $CFLAGS -D_GNU_SOURCE -I"$LIBS/libkbase" -I"$LIBS/libkcolor" \
	-I"$LIBS/libkwm" \
	-c "$LIBS"/libkbase/*.c "$LIBS"/libkcolor/*.c "$LIBS"/libkwm/*.c \
	&& ar rcs libkdos.a *.o)
export CFLAGS="$CFLAGS -I$LIBS/libkcolor -I$LIBS/libkbase -I$LIBS/libkwm"
export LDFLAGS="$LDFLAGS $PWD/klibs/libkdos.a"

meson setup build "$PORT_SRC" --prefix=/usr --libdir=lib --buildtype=release \
      -Dxwayland=enabled -Dicon=disabled -Dsvg=disabled -Dnls=disabled \
      -Dman-pages=disabled -Dlabnag=disabled -Dsystemd-session=disabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

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

# libkcolor for the CRT pass, libkwm for the window model, and the libkbase
# substrate under both — static, linking nothing but musl, compiled here and fed
# to meson through the environment. libkwm is what makes placement, tiling and
# the edge search ONE implementation shared with kdos-con rather than two.
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

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

# libkcolor and the libkbase substrate under it, static and linking nothing but
# musl, compiled here and fed to meson through the environment — the shape
# kdos-comp's recipe already uses. This is the ONLY KDOS library this binary
# links: a kiosk compositor draws no cells, so it wants the palette and nothing
# else from the toolkit.
LIBS="$PORT_SRC/../../libs"
mkdir -p klibs
(cd klibs && gcc $CFLAGS -D_GNU_SOURCE -I"$LIBS/libkbase" -I"$LIBS/libkcolor" \
	-c "$LIBS"/libkbase/*.c "$LIBS"/libkcolor/*.c \
	&& ar rcs libkdos.a *.o)
export CFLAGS="$CFLAGS -I$LIBS/libkcolor -I$LIBS/libkbase"
export LDFLAGS="$LDFLAGS $PWD/klibs/libkdos.a"

meson setup build "$PORT_SRC" --prefix=/usr --libdir=lib --buildtype=release
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

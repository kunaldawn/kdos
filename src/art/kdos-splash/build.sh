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

# Static: this runs from the initramfs, before any real root exists.
#
# libkcolor is on the INCLUDE path and not the link line. The splash expands
# KCOL_SCHEMES into a table of its own so the palette cannot drift from the
# rest of the distro, and still links nothing: a shared object here would be a
# shared object the initramfs has to carry.
LIBS="$PORT_SRC/../../libs"
gcc $CFLAGS -O2 -Wall -static -I"$LIBS/libkcolor" \
	-o kdos-splash "$PORT_SRC/kdos-splash.c" -lm
install -Dm755 kdos-splash "$PKG/usr/bin/kdos-splash"

# The splash renders text with the console font. Ship it decompressed so
# the program needs no zlib.
gzip -dc /usr/share/consolefonts/ter-v16n.psf.gz > splash.psf
install -Dm644 splash.psf "$PKG/usr/share/kdos/splash.psf"

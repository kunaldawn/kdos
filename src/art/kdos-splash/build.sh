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
gcc $CFLAGS -O2 -Wall -static-pie -I"$LIBS/libkcolor" \
	-o kdos-splash "$PORT_SRC/kdos-splash.c" -lm $LDFLAGS
install -Dm755 kdos-splash "$PKG/usr/bin/kdos-splash"

gzip -dc /usr/share/consolefonts/ter-v16n.psf.gz > splash.psf
install -Dm644 splash.psf "$PKG/usr/share/kdos/splash.psf"

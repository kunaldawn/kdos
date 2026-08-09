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
gcc $CFLAGS -O2 -Wall -static -o kdos-splash "$PORT_SRC/kdos-splash.c" -lm
install -Dm755 kdos-splash "$PKG/usr/bin/kdos-splash"

# The splash renders text with the console font. Ship it decompressed so
# the program needs no zlib.
gzip -dc /usr/share/consolefonts/ter-v16n.psf.gz > splash.psf
install -Dm644 splash.psf "$PKG/usr/share/kdos/splash.psf"

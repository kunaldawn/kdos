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

# SCPI OVER ETHERNET IS HOW A MODERN INSTRUMENT IS SCRIPTED. A scope or a
# supply made in the last fifteen years answers text commands on port 5025 —
# `lxi scpi "MEAS:VOLT:DC?"` — and `lxi discover` finds them by mDNS, which
# avahi is already running for. That turns a bench into something a shell
# script can sweep, log and plot with gnuplot.
#
# -Dgui=false: the GUI is GTK4 and libadwaita, which is the hard rule twice
# over. The CLI is the whole feature.
meson setup build \
	--prefix=/usr \
	--libdir=lib \
	--buildtype=release \
	-Dgui=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

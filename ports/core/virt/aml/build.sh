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

# The only option this project defines is `examples`, and meson fails at setup
# on anything else. The examples are three demo programs nothing links.
#
# Upstream installs the header as <aml1/aml.h> and names the pkg-config file
# aml1.pc, with the soname taken from the library version: neatvnc and wayvnc
# ask for `aml1` version >= 1.0.0 < 2.0.0, so the versioned names are the
# interface and a build that flattens them leaves both consumers unbuildable.
meson setup build \
	--prefix=/usr \
	--libdir=lib \
	--buildtype=release \
	-Dexamples=false

meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

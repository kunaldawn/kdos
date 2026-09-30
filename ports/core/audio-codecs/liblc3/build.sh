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

# The whole option set this project defines is `tools` and `python`, both
# already false. They are named anyway: meson aborts at setup on an option it
# does not know, so a spelling checked against the tarball is the only one
# worth shipping, and a default that moves upstream would otherwise change
# what this image installs without the recipe saying so.
#
# `python` builds CPython bindings and would put a build-time interpreter and
# a runtime extension module in the dependency closure of a codec whose only
# consumer is pipewire's bluez5 plugin.
#
# `tools` builds `elc3` and `dlc3`, an encoder and a decoder on the command
# line. Nothing on the image drives them, and both names are short enough to
# collide with something a person expects on $PATH.
#
# --default-library=shared is what pipewire's meson looks for: it resolves
# liblc3 through pkg-config and dlopens nothing, so a static-only build gives
# a `lc3.pc` that satisfies the probe and a bluez5 plugin that cannot link.
meson setup build \
	--prefix=/usr --libdir=lib \
	--buildtype=release \
	--default-library=shared \
	-Dtools=false \
	-Dpython=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

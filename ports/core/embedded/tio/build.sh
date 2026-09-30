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

# IT RECONNECTS, WHICH IS THE ENTIRE DIFFERENCE FROM picocom. A dev board being
# flashed disappears from USB and comes back a second later with a new device
# node; picocom exits and has to be restarted every single time, tio waits and
# reattaches. Over a session of flash-test-flash that is the difference between
# one terminal and a hundred restarts.
#
# Both ship: picocom is the small one that is always there and reads a dead
# router's console; this is the one for an edit-compile-flash loop.
#
# The completion directory is named because meson otherwise installs the
# completion only when it finds bash-completion's pkg-config file, which this
# port does not depend on.
meson setup build \
	--prefix=/usr \
	--libdir=lib \
	--buildtype=release \
	-Dbashcompletiondir=/usr/share/bash-completion/completions
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

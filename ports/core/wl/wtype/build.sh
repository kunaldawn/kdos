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

# The keys reach the focused window through zwp_virtual_keyboard_v1, so wtype
# types only where the compositor offers that global; elsewhere it exits with
# an error and types nothing. The keymap it uploads is built per call from
# the characters asked for, which is how it types symbols no layout has.
# meson.build builds the version string from git describe whenever git is on
# PATH; with no repository reachable git fails and the string is fixed, where
# an enclosing checkout would stamp its own describe into the binary.
GIT_DIR=/nonexistent meson setup build \
	--prefix=/usr --libdir=lib \
	--buildtype=release
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

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

# THE SCANNER ON THE NETWORK HAS NO DRIVER AND DOES NOT NEED ONE. Every device
# made in the last decade speaks eSCL or WSD over IP and announces itself with
# mDNS, which avahi is already running — so this finds it without a vendor
# blob, which is the whole reason the sane-backends list stops being the
# question.
meson setup build --prefix=/usr --libdir=lib --buildtype=release
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

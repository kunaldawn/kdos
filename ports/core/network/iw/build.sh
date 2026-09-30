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

# THIS IS A PREREQUISITE, NOT A CONVENIENCE. hostapd cannot be brought up
# without knowing whether the card does AP mode, and aircrack-ng needs monitor
# mode set — both are `iw` and there is no other way to ask nl80211 those
# questions from a shell. It is also what answers "why will this card not
# associate at 5 GHz", which on a machine whose regdb must stay upstream-signed
# is a question that comes up.
#
# The makefile has no configure; PREFIX and the pkg-config lookups are all it
# takes. V=1 keeps the compile lines in the port log.
make V=1 PREFIX=/usr
make V=1 PREFIX=/usr DESTDIR=$PKG install

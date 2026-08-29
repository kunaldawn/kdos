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

# `qalc` IS THE DELIVERABLE AND THE GUI IS NOT BUILT — that is a separate GTK
# program and the hard rule covers it. What this gives is a prompt that takes
# `(52 mph to km/h) * 3 hours` and answers, with unit conversion, exact
# rationals, symbolic simplification and a currency table.
#
# THE CURRENCY RATES ARE THE ONE ONLINE FEATURE AND THEY FAIL QUIETLY. qalc
# fetches an exchange-rate file on demand; with no network it uses whatever is
# in ~/.local/share/qalculate and, absent that, refuses a currency conversion
# rather than inventing one. Every other unit — length, mass, energy, data — is
# a compiled-in definition and works forever.
#
# It sits beside numbat rather than replacing it: numbat REFUSES a nonsensical
# unit combination by construction, and this one is the more forgiving
# calculator with a much larger definition set.
#
# --with-icu, not --without-icu=no: autoconf's --without-PACKAGE takes no value
# and reads `icu=no` as the package NAME, which it refuses outright. ICU is a
# port and is what makes unit names and output normalise the same way here as
# everywhere else in the tree.
./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--disable-static \
	--with-icu \
	--enable-textport
make
make DESTDIR=$PKG install

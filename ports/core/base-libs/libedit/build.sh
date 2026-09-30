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

# The terminal capability database comes from ncurses: configure walks
# ncurses, curses, termcap, tinfo looking for tgetent and stops at the first
# one, so ncurses must be installed or the search ends in an error rather than
# a narrower library.
#
# The examples are noinst test drivers, compiled and then never installed, so
# building them is time spent on files the package does not contain.
./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--disable-static \
	--disable-examples
make
make DESTDIR=$PKG install

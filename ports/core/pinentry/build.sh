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

# Every graphical front end is off: GTK and Qt by rule, EFL and FLTK because
# neither is a port. Most default to `maybe`, and would otherwise appear the
# moment their toolkit happened to be installed.
./configure --prefix=/usr \
	--enable-pinentry-curses \
	--enable-pinentry-tty \
	--disable-pinentry-emacs \
	--disable-pinentry-efl \
	--disable-pinentry-gtk2 \
	--disable-pinentry-gnome3 \
	--disable-pinentry-qt \
	--disable-pinentry-qt5 \
	--disable-pinentry-qt4 \
	--disable-pinentry-tqt \
	--disable-pinentry-fltk \
	--disable-libsecret
make
make DESTDIR=$PKG install

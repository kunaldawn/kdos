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

# THE `dumb` TERMINAL DRAWS WHERE NO WINDOW CAN OPEN. gnuplot draws into a
# character grid, so a plot is something you get at a prompt over ssh or on
# tty1. `sixelgd` is the richer version of the same thing inside kdos-term or
# foot, and it exists only with libgd — as do the png, jpeg and animated gif
# terminals.
#
# THE qt TERMINAL IS THE INTERACTIVE WINDOW: zoom, pan and rotate with the
# mouse, drawn by gnuplot_qt, which Qt puts on Wayland. It wants Core5Compat
# and Svg besides qtbase, and lrelease from qttools for its translations.
# configure asks pkg-config for all of them and, when one is missing, drops the
# terminal with nothing but a line in its summary, so the check after the
# configure is what makes a missing Qt a failed build. The same holds for
# libcerf, whose complex error functions (cerf, erfi, the Voigt profile) are
# built in only when configure finds it.
#
# NO wxt TERMINAL: it is a second interactive window beside qt, and it would
# bring wxWidgets, and through it WebKitGTK and GStreamer, into this port's
# build.
./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--with-readline=gnu \
	--with-qt=qt6 \
	--without-wx \
	--without-x \
	--with-bitmap-terminals \
	--with-gd \
	--with-cairo \
	--with-lua \
	--with-libcerf \
	--without-amos \
	--without-caca \
	--without-latex \
	--enable-history-file
for def in QTTERM HAVE_LIBCERF; do
	grep -q "^#define $def 1" config.h || { echo "gnuplot: $def not configured" >&2; exit 1; }
done
make
make DESTDIR=$PKG install

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


# A plain makefile with no configure.
#
# LIBDIR IS RELATIVE TO PREFIX AND NOT A PATH. The makefile writes to
# `$(DESTDIR)$(PREFIX)/$(LIBDIR)`, so an absolute `/usr/lib` lands the library
# and its pkg-config file in `/usr/usr/lib` — where nothing looks, and where
# the symptom is pipewire's meson reporting `Dependency "libfreeaptx" not
# found` while the file plainly exists.
make PREFIX=/usr LIBDIR=lib CFLAGS="${CFLAGS/-O2/-O3}" LDFLAGS="$LDFLAGS"
make DESTDIR=$PKG PREFIX=/usr LIBDIR=lib install

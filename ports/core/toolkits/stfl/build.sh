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

# TWO THINGS THE HEADER LAYOUT COSTS, and neither is an edit to the source.
#
# stfl includes <ncursesw/ncurses.h>. This tree builds ONE ncurses, widec, and
# installs its headers flat in /usr/include with no ncursesw directory, so the
# include resolves nowhere and the first object fails to compile. A directory
# of one symlink puts the header where the source looks for it.
#
# And that header declares the wide-character functions — wget_wch,
# mvwaddnwstr, the ones stfl is written against — only when NCURSES_WIDECHAR
# is 1, which it sets from _XOPEN_SOURCE_EXTENDED. Without the define they are
# implicit declarations against a widec library: the narrow API from the wide
# build, which is a link error at best and the wrong call at worst.
#
# CFLAGS is given on the COMMAND LINE, which replaces the Makefile's own
# `CFLAGS += … -Os -ggdb …` rather than being extended by it, so no object
# carries debug information. It therefore carries the rest of that line
# itself: without -fPIC the shared library does not link.
mkdir -p compat/ncursesw
ln -sf /usr/include/ncurses.h compat/ncursesw/ncurses.h
_cflags="$CFLAGS -I. -I$PWD/compat -Wall -D_GNU_SOURCE -D_XOPEN_SOURCE_EXTENDED -fPIC"

# libdir is joined to prefix by the Makefile, so it is a leaf and not a path.
#
# Every binding is named off. FOUND_SWIG=0 drops perl, python and ruby, which
# go through SWIG; SPL has its own probe.
make FOUND_SWIG=0 FOUND_SPL=0 CFLAGS="$_cflags"
make FOUND_SWIG=0 FOUND_SPL=0 CFLAGS="$_cflags" \
	prefix=/usr libdir=lib DESTDIR=$PKG install

# The install target creates the LINKER name and not the SONAME. The library
# is built -Wl,-soname,libstfl.so.0, so that is the name a program linked
# against it asks the loader for, and nothing on this system runs ldconfig to
# create it: without this link newsboat links cleanly and then does not start.
ln -sf libstfl.so.$version $PKG/usr/lib/libstfl.so.0

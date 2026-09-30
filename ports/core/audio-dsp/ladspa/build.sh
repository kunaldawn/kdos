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

cd src
patch -p1 -i "$PORT_SRC/fix-memleak-in-plugin-scanning.patch"

# The makefile's CFLAGS is its configuration: it carries the include path and
# DEFAULT_LADSPA_PATH, the directory the tools search when LADSPA_PATH is
# unset. It is also the only flag variable: it ignores the exported CFLAGS,
# names no LDFLAGS, and every compile and link line reads it (CXXFLAGS copies
# it). So CFLAGS and CXXFLAGS are given on the command line as the exported
# flags, then the makefile's own entries, kept as its variable references in
# $mk so the path is still the makefile's, then LDFLAGS for the link lines; the
# compiler drops -Wl, options on a compile-only line. The install directories
# are redirected afterwards, when everything is already compiled.
mk='$(INCLUDES) -Wall -Werror -fPIC -DDEFAULT_LADSPA_PATH=$(INSTALL_PLUGINS_DIR)'
make targets CFLAGS="$CFLAGS $mk $LDFLAGS" CXXFLAGS="$CXXFLAGS $mk $LDFLAGS"
make INSTALL_PLUGINS_DIR="$PKG/usr/lib/ladspa/" \
	INSTALL_INCLUDE_DIR="$PKG/usr/include/" \
	INSTALL_BINARY_DIR="$PKG/usr/bin/" install

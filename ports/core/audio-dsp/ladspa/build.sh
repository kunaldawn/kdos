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

mk='$(INCLUDES) -Wall -Werror -fPIC -DDEFAULT_LADSPA_PATH=$(INSTALL_PLUGINS_DIR)'
make targets CFLAGS="$CFLAGS $mk $LDFLAGS" CXXFLAGS="$CXXFLAGS $mk $LDFLAGS"
make INSTALL_PLUGINS_DIR="$PKG/usr/lib/ladspa/" \
	INSTALL_INCLUDE_DIR="$PKG/usr/include/" \
	INSTALL_BINARY_DIR="$PKG/usr/bin/" install

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
# unset. Build with the makefile's own values, and redirect only the install
# directories afterwards, when everything is already compiled.
make targets
make INSTALL_PLUGINS_DIR="$PKG/usr/lib/ladspa/" \
	INSTALL_INCLUDE_DIR="$PKG/usr/include/" \
	INSTALL_BINARY_DIR="$PKG/usr/bin/" install

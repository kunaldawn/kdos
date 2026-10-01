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

# config.mk ASSIGNS CFLAGS AND LDFLAGS AND THEREFORE WINS, which is why this is
# the one place flags go on the command line rather than into the environment:
# a makefile's own assignment beats an exported variable, and upstream's are
# `-g -O0` and `-L/usr/lib`, so the exported flags would be silently dropped.
# The command line carries the exported flags and the rest of upstream's set —
# `-ansi -Werror` is how this program is meant to be built and it compiles
# clean; `-ansi` comes after the exported `-std=gnu11` and wins.
make PREFIX=/usr CFLAGS="$CFLAGS -Wall -Werror -ansi -I. -DVERSION=\"$version\"" \
	LDFLAGS="$LDFLAGS"
make DESTDIR=$PKG PREFIX=/usr install

# NO DESKTOP ENTRY. smu is a filter: `smu README.md > README.html`. A launcher
# with no file opens a program reading a terminal nobody is typing into.

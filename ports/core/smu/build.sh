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

# config.mk ASSIGNS CFLAGS AND THEREFORE WINS, which is why this is the one
# place a flag goes on the command line rather than into the environment: a
# makefile's own assignment beats an exported variable, and upstream's is
# `-g -O0`, so an exported -O2 would be silently dropped. The rest of its flag
# set is kept — `-ansi -Werror` is how this program is meant to be built and it
# compiles clean.
make PREFIX=/usr CFLAGS="-O2 -Wall -Werror -ansi -I. -DVERSION=\"$version\""
make DESTDIR=$PKG PREFIX=/usr install

# NO DESKTOP ENTRY. smu is a filter: `smu README.md > README.html`. A launcher
# with no file opens a program reading a terminal nobody is typing into.

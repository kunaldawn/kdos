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

# The makefile folds CFLAGS into CC for both of its sub-makes, and its own
# CFLAGS is only an optimisation level, so the tree's flags are passed as
# arguments. DESTDIR is the makefile's install prefix, not a staging root.
# One job, for the build and for the install, which remakes `all`: `all`
# lists a target that deletes ./xa beside the one that links it, and run in
# parallel the delete can land after the link.
make -j1 CFLAGS="$CFLAGS" LDFLAGS="$LDFLAGS"
make -j1 install CFLAGS="$CFLAGS" LDFLAGS="$LDFLAGS" DESTDIR="$PKG/usr"

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

# TOYBOX sed IS POSIX AND UPSTREAM BUILD SYSTEMS ARE NOT. This is the third
# GNU tool to override a toybox applet for that reason — `gawk` and
# `findutils` are already here — and the case that forced it is worth
# recording: xapian's configure generates its public `version.h` through a sed
# script using `0,/regex/d`, a GNU address form toybox does not implement.
# Toybox produced NOTHING, configure does not check, and the result was a
# ZERO-BYTE version.h, a build that got as far as the compiler, and an error
# about a type that does not name a type. Nothing anywhere said "sed".
#
# That is the shape of every one of these: a GNU extension used silently, and
# a failure several steps downstream that names something else entirely.
#
# It installs over toybox's symlink — whoever comes last in the dependency
# order wins, which is the build's rule, and kpkg's --overwrite is what lets
# the path change hands cleanly.
./configure \
	--prefix=/usr \
	--bindir=/bin \
	--disable-nls \
	--without-selinux
make
make DESTDIR=$PKG install

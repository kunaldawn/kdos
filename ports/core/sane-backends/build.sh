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

# THE VERSION COMES FROM A FILE THAT ONLY THE RELEASE TARBALL HAS. configure.ac
# computes it with `tools/git-version-gen --prefix '' .tarball-version`, and
# this is a GitLab tag ARCHIVE — no .tarball-version and no git repository — so
# VERSION becomes the literal string UNKNOWN. The build then compiles every
# backend with `-DV_MAJOR=UNKNOWN -DV_MINOR=`, which fails inside
# SANE_VERSION_CODE with an undeclared identifier rather than anywhere that
# names a version.
echo "$version" > .tarball-version

./autogen.sh
# saned's own network protocol stays out: an unauthenticated scanner daemon on
# a workstation is an open port for a feature nothing here starts.
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--disable-static --with-usb --without-snmp --disable-locking
make
make DESTDIR=$PKG install

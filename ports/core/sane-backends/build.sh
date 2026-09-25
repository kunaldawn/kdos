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
#
# Every optional library is named, because each defaults to "use it if it is
# there" and would otherwise follow build order. avahi and libcurl carry the
# escl backend and network discovery, libxml2 USB record/replay, and libv4l1
# the v4l backend. gphoto2 stays out: importing from a camera is gphoto2's own
# job. poppler-glib stays out because poppler is built without its glib
# binding.
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--disable-static --with-usb --without-snmp --disable-locking \
	--with-avahi --with-libcurl --with-usb-record-replay --with-v4l \
	--without-gphoto2 --without-poppler-glib --without-systemd
make
make DESTDIR=$PKG install

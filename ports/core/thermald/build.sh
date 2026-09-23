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


# THE TARBALL IS A GIT SNAPSHOT and ships no configure.
#
# --with-systemdsystemunitdir=no drops the unit and the D-Bus activation file
# that starts it; the daemon is started by an init script like every other.
# `m4/` IS NOT IN THE TARBALL AND autoreconf REQUIRES IT. configure.ac calls
# GTK_DOC_CHECK, so autoreconf runs gtkdocize, which copies its makefile into
# m4/ and fails on a directory that is not there — reported as
# `gtkdocize failed`, which points at gtk-doc rather than at a missing mkdir.
mkdir -p m4
autoreconf -fi
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--localstatedir=/var \
	--with-systemdsystemunitdir=no \
	--with-dbus-sys-dir=/usr/share/dbus-1/system.d
make
make DESTDIR=$PKG install

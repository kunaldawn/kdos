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

# THE SCANNER TABLE REACHES udev AS A hwdb FILE AND A RULE, AND make install
# CARRIES NEITHER. tools/sane-desc is built by make and turns the backends'
# .desc files into both: 20-sane.hwdb sets libsane_matched on every USB id a
# backend supports, and 65-libsane.rules sets it on the SCSI scanners and keeps
# a matched device out of autosuspend. fs/etc/udev/rules.d/70-kdos-scanner.rules
# turns the mark into the dialout grant. The udev+hwdb form and not plain udev:
# the plain one writes GROUP="scanner" on every line, a group this system does
# not have, which eudev resolves to gid 0. kpkg's hwdb trigger compiles the
# hwdb file into /etc/udev/hwdb.bin; without that trie no USB scanner is
# marked.
descs=doc/descriptions:doc/descriptions-external
install -d "$PKG/usr/lib/udev/rules.d" "$PKG/usr/lib/udev/hwdb.d"
tools/sane-desc -m udev+hwdb -s $descs -d 0 > "$PKG/usr/lib/udev/rules.d/65-libsane.rules"
tools/sane-desc -m hwdb -s $descs -d 0 > "$PKG/usr/lib/udev/hwdb.d/20-sane.hwdb"

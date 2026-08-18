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

# Upstream's tag archive, not IANA's tzdata/tzcode pair.
#
# kpkg passes --strip-components=1 to the first source unconditionally, so a
# source must have a top-level directory. data.iana.org ships tzdata<ver> and
# tzcode<ver> as two FLAT tarballs; extracting either that way discards every
# top-level file and leaves only the contents of subdirectories, which
# presents as a missing Makefile. The git tag archive carries code and data
# together under one directory.

# zic is built and RUN here; it is not installed. The target reads the compiled
# binary database and musl parses those files itself — there is no libc
# timezone helper that needs to ship beside them.
make CFLAGS="$CFLAGS -DHAVE_GETTEXT=0" zic

# `backward` carries the historical aliases (US/Eastern, Asia/Calcutta) that a
# great deal of software and a great many user configs still name. Dropping it
# saves ~100 KB and breaks a config the user did not write.
ZONES="africa antarctica asia australasia europe northamerica southamerica \
       etcetera backward factory"

# -b fat, not -b slim. Slim files are smaller and are read correctly only by a
# recent libc; musl handles them, but the zoneinfo tree is also read by the
# appbox's glibc programs through the shared filesystem, and a format the box
# misreads is a host and box that disagree about local time — which is the
# exact defect this port exists to close.
install -dm755 "$PKG/usr/share/zoneinfo"
./zic -b fat -d "$PKG/usr/share/zoneinfo" $ZONES
./zic -b fat -d "$PKG/usr/share/zoneinfo/right" -L leapseconds $ZONES

install -m644 zone.tab zone1970.tab iso3166.tab "$PKG/usr/share/zoneinfo/"
install -m644 leapseconds "$PKG/usr/share/zoneinfo/"

# UTC is the honest default for a machine that has not been told where it is;
# kinstall replaces this symlink with the zone the user picked.
install -dm755 "$PKG/etc"
ln -sf /usr/share/zoneinfo/UTC "$PKG/etc/localtime"

install -dm755 "$PKG/usr/share/licenses/$name"
install -m644 LICENSE "$PKG/usr/share/licenses/$name/"

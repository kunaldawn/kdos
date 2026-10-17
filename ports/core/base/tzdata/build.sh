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

make CC=cc AWK=gawk CFLAGS="$CFLAGS -DHAVE_GETTEXT=0" VERSION=$version zic leapseconds tzdata.zi

ZONES="africa antarctica asia australasia europe northamerica southamerica \
       etcetera backward factory"

install -dm755 "$PKG/usr/share/zoneinfo"
./zic -b fat -d "$PKG/usr/share/zoneinfo" $ZONES
./zic -b fat -d "$PKG/usr/share/zoneinfo/right" -L leapseconds $ZONES

install -m644 zone.tab zone1970.tab iso3166.tab "$PKG/usr/share/zoneinfo/"
install -m644 leapseconds tzdata.zi "$PKG/usr/share/zoneinfo/"
install -Dm644 tzfile.5 -t "$PKG/usr/share/man/man5"

install -dm755 "$PKG/usr/share/licenses/$name"
install -m644 LICENSE "$PKG/usr/share/licenses/$name/"

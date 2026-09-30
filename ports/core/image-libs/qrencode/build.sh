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

autoreconf -f -i

# `-t ANSI` draws the code out of block characters, which is the whole reason
# this belongs on a machine whose desktop is a cell grid: a Syncthing device
# id, a WiFi credential or an F-Droid URL can leave this machine through a
# phone camera with no network between them.
# --with-tools, not --without-tools=NO: the option is one AC_ARG_WITH and
# autoconf reads everything after `--without-` as the PACKAGE NAME, so the
# second spelling asks for a package called `tools=NO` and configure refuses
# it. The CLI is the point of the port; the library alone has no consumer here.
# --with-png only asks: a failed libpng check is a configure warning and a CLI
# with no `-t PNG`, so libpng on the depends line is what holds it on.
./configure --prefix=/usr --libdir=/usr/lib --disable-static --with-tools \
	--with-png
make
make DESTDIR=$PKG install

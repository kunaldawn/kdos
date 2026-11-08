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

SASSC_VERSION=$version autoreconf -fi
./configure --prefix=/usr --disable-static
make
make DESTDIR=$PKG install

help2man --no-info --name="compile Sass stylesheets to CSS" \
	--version-string="sassc $version" -o sassc.1 ./sassc
install -Dm644 sassc.1 -t "$PKG/usr/share/man/man1"

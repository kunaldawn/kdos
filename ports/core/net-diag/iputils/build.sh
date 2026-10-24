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

meson setup build --prefix=/usr --buildtype=release \
	-DNO_SETCAP_OR_SUID=true -DUSE_GETTEXT=false -DUSE_IDN=false -DBUILD_MANS=false \
	-DSKIP_TESTS=true
ninja -C build
DESTDIR=$PKG ninja -C build install
install -Dm644 doc/*.8 -t "$PKG/usr/share/man/man8"

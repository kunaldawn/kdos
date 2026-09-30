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

# webkitgtk=false: WebKit serves only --text-info --html, and a dialog helper
# that pulls in a browser engine is the wrong trade. The manual page is made
# by running the built zenity under help2man.
meson setup build --prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	-Dwebkitgtk=false \
	-Dmanpage=true
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

# Bundled data is English only: GTK falls back to the source strings, and the
# Yelp manual keeps its C pages.
rm -rf "$PKG/usr/share/locale"
find "$PKG/usr/share/help" -mindepth 1 -maxdepth 1 ! -name C -exec rm -rf {} +

test -x "$PKG/usr/bin/zenity"
test -f "$PKG/usr/share/man/man1/zenity.1"

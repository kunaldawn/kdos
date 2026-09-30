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

# The playlist parser Rhythmbox and Brasero read .m3u, .pls, .xspf and podcast
# feeds with. libarchive lets it look inside an ISO for a disc title, libgcrypt
# decodes Amazon .amz playlists, uchardet guesses a playlist's character set;
# each is "auto" upstream and named on here. The typelib is built for
# Rhythmbox's Python plugins.
meson setup build --prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	-Denable-libarchive=yes \
	-Denable-libgcrypt=yes \
	-Denable-uchardet=yes \
	-Denable-gtk-doc=false \
	-Dintrospection=true
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

# Bundled data is English only; the library falls back to its source strings.
rm -rf "$PKG/usr/share/locale"

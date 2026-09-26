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

# An unencrypted disc or a backup plays through mpv's bd:// and ffmpeg's
# bluray: protocol. An AACS-encrypted disc needs libaacs, which is not a port.
#
# -Dbdj_jar=disabled: BD-J menus are Java, and the jar needs a JDK to build and
# a JVM to run, neither on the host. A disc then plays its titles without the
# Java menu. freetype and fontconfig draw the text subtitles, and libxml2 reads
# the disc's metadata.
meson setup build --prefix=/usr --libdir=lib --buildtype=release \
	-Ddefault_library=shared \
	-Dbdj_jar=disabled \
	-Dfreetype=enabled \
	-Dfontconfig=enabled \
	-Dlibxml2=enabled \
	-Denable_tools=true \
	-Denable_devtools=false \
	-Denable_examples=false \
	-Denable_docs=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

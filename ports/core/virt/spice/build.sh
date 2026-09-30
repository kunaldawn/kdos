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

# The server side of the protocol only: spice-gtk is the client, and QEMU's
# --enable-spice is what reaches this library.
#
# GStreamer carries the video streaming encoders (VP8, VP9 and H.264 through
# whatever plugins are installed) and falls back to MJPEG without them.
# libcacard is the smartcard channel a guest's virtual reader rides.
#
# spice-common is a subproject in the tarball; its code generator is a Python
# script that needs pyparsing, so python3 is a build dependency here too.
# -Dmanual=false skips the asciidoc HTML manual.
meson setup build --prefix=/usr --libdir=lib --buildtype=release \
	--wrap-mode=nodownload \
	-Dgstreamer=1.0 \
	-Dlz4=true \
	-Dsasl=true \
	-Dopus=enabled \
	-Dsmartcard=enabled \
	-Dmanual=false \
	-Dtests=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

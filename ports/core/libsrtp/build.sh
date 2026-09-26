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

# gst-plugins-bad's srtpenc and srtpdec, and with them webrtcbin, link this.
# The ciphers come from openssl: the built-in ones have no AES-GCM, and WebRTC
# offers GCM first. The key derivation stays libsrtp's own, so the library
# depends on nothing in openssl beyond the ciphers.
meson setup build --prefix=/usr --libdir=lib --buildtype=release \
	-Dcrypto-library=openssl \
	-Dcrypto-library-kdf=disabled \
	-Dtests=disabled \
	-Dpcap-tests=disabled \
	-Dfuzzer=disabled \
	-Ddoc=disabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

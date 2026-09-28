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

# Every optional library is found with required: false and silently dropped
# when absent, so each one is in depends: libgcrypt for the DH/DES the VeNCrypt
# and Apple logins use, GnuTLS for TLS, cyrus-sasl, libjpeg for Tight, libpng,
# LZO and zlib for their encodings. With a subprojects/aml directory present
# meson would build a private static aml; there is none, so the aml port links.
meson setup build \
	--prefix=/usr --libdir=lib \
	--buildtype=release
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

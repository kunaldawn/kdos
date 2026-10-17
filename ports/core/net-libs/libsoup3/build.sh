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

meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	-Dgssapi=enabled \
	-Dntlm=enabled \
	-Dntlm_auth=/usr/bin/ntlm_auth \
	-Dbrotli=enabled \
	-Dzstd=enabled \
	-Dtls_check=false \
	-Dintrospection=enabled \
	-Dvapi=disabled \
	-Ddocs=disabled \
	-Dtests=false \
	-Dautobahn=disabled \
	-Dinstalled_tests=false \
	-Dsysprof=disabled \
	-Dfuzzing=disabled \
	-Dpkcs11_tests=disabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

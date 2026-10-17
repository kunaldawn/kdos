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

meson setup build --prefix=/usr --libdir=lib --buildtype=release \
	-Dgenpeimg=disabled \
	-Defi_sbat_distro_id=kdos \
	-Defi_sbat_distro_summary=KDOS \
	-Defi_sbat_distro_pkgname=$name \
	-Defi_sbat_distro_version=$version-$release \
	-Defi_sbat_distro_url=https://github.com/kunaldawn/kdos
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

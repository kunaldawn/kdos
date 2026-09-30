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

# SMART is an ATA interface and says nothing about an NVMe, whose wear,
# media errors and thermal throttling live in log pages only this reads.
# libnvme is built from this tree and installs under the libnvme3 name.
#
# -Dnvmf-autoconnect=disabled: its udev rules, dracut snippets and
# dispatcher scripts start systemd units. -Ddocs=man with -Ddocs-build=false
# installs the pages prebuilt in the archive. -Dpython=disabled keeps the
# bindings from appearing whenever swig happens to be installed. json-c is what makes
# `-o json` work, which is what turns this into something a script can read.
#
# json-c, libkmod, openssl and keyutils default to auto and are forced on: a
# missing one otherwise drops `-o json`, the fabrics module loading or the
# TLS PSK support without a word. json-c and openssl carry meson subproject
# fallbacks, and --wrap-mode=nodownload makes a missing one fail setup
# instead of fetching it.
meson setup build \
	--prefix=/usr \
	--sysconfdir=/etc \
	--libdir=lib \
	--buildtype=release \
	--wrap-mode=nodownload \
	-Djson-c=enabled \
	-Dlibkmod=enabled \
	-Dopenssl=enabled \
	-Dkeyutils=enabled \
	-Dnvmf-autoconnect=disabled \
	-Dpython=disabled \
	-Ddocs=man \
	-Ddocs-build=false \
	-Dtests=false \
	-Dexamples=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

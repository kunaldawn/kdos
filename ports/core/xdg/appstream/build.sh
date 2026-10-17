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

cd "AppStream-$version"

meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	-Dsystemd=false \
	-Dstemming=true \
	-Dqt=false \
	-Dcompose=false \
	-Dvapi=false \
	-Dgir=true \
	-Dtools=true \
	-Dzstd-support=true \
	-Dblake3-support=false \
	-Dapt-support=false \
	-Ddisplay-detection=wayland \
	-Dbash-completion=true \
	-Ddocs=false \
	-Dapidocs=false \
	-Dinstall-docs=false \
	-Dman=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

rm -rf "$PKG/usr/share/installed-tests"

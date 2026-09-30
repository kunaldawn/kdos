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

# The tarball wraps its tree as ./AppStream-<version>/, so the one-component
# strip leaves it a level down.
cd "AppStream-$version"

# systemd is off by rule. stemming needs libstemmer, which is not a port, and
# only ranks search results. The Qt binding, the compose library and the Vala
# API have no consumer here. display-detection=wayland is the display-size
# check behind <requires><display_length>, reading wayland-client at run time.
# The manual page is off: its stylesheet is the namespaced DocBook XSL
# release, which docbook-xsl does not ship, and setup stops without it.
meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	-Dsystemd=false \
	-Dstemming=false \
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

# installed-tests has no switch; the tests need the build tree and are no
# use on the target.
rm -rf "$PKG/usr/share/installed-tests"

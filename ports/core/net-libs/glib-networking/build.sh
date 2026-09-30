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

# GnuTLS, upstream's primary backend; its trust store is p11-kit's, which
# reads the same ca-certificates bundle everything else uses. The module lands
# in /usr/lib/gio/modules, and installing the package rebuilds giomodule.cache.
#
# The proxy resolvers: environment_proxy honours http_proxy and friends, which
# is how a proxy is configured here. gnome_proxy reads the org.gnome.system.proxy
# schema, which nothing on this host installs, and GSettings aborts the process
# that asks for a schema that is not there. libproxy is not a port.
meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	--libexecdir=/usr/lib \
	--buildtype=release \
	-Dgnutls=enabled \
	-Dopenssl=disabled \
	-Denvironment_proxy=enabled \
	-Dlibproxy=disabled \
	-Dgnome_proxy=disabled \
	-Dtests=false \
	-Dinstalled_tests=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

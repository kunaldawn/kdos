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

# The GObject plugin engine Rhythmbox loads its C and Python plugins through,
# with the GTK 3 widgetry for its plugin preferences. The 1.x series is what
# Rhythmbox 3.5 asks for (libpeas-1.0); libpeas 2 is a different API.
#
# The Lua 5.1 loader needs lua-lgi and Glade's catalog needs gladeui, neither
# a port; Python 2 does not exist here. The demos are example programs.
meson setup build --prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	-Db_ndebug=if-release \
	-Dpython3=true \
	-Dpython2=false \
	-Dlua51=false \
	-Dintrospection=true \
	-Dvapi=true \
	-Dwidgetry=true \
	-Dglade_catalog=false \
	-Ddemos=false \
	-Dgtk_doc=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

# Bundled data is English only; the library falls back to its source strings.
rm -rf "$PKG/usr/share/locale"

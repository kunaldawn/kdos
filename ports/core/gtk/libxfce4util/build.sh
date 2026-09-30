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

# The Xfce utility library under xfconf, libxfce4ui and exo, which Xfburn
# needs. Introspection and the Vala bindings are off: nothing here binds the
# Xfce libraries from another language.
meson setup build --prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	-Db_ndebug=if-release \
	-Dgtk-doc=false \
	-Dintrospection=false \
	-Dvala=disabled \
	-Dvisibility=true
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

# Bundled data is English only; the library falls back to its source strings.
rm -rf "$PKG/usr/share/locale"

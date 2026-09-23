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

# Introspection is off. Turning it on takes gobject-introspection and
# glib-introspection in depends: g-ir-scanner needs GLib-2.0.gir,
# GObject-2.0.gir and Gio-2.0.gir, and glib-introspection is what installs them.
meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	-Dintrospection=disabled \
	-Dnls=enabled \
	-Ddocumentation=disabled \
	-Dgtk_doc=disabled \
	-Dman=true \
	-Dtests=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

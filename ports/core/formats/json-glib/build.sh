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

# introspection installs Json-1.0.gir and its typelib, for the GIRs that
# include it and for PyGObject programs. g-ir-scanner needs GLib-2.0.gir,
# GObject-2.0.gir and Gio-2.0.gir, and glib-introspection is what installs
# them.
meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	-Dintrospection=enabled \
	-Dnls=enabled \
	-Ddocumentation=disabled \
	-Dgtk_doc=disabled \
	-Dman=true \
	-Dtests=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

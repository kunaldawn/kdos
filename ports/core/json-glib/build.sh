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

# INTROSPECTION STAYS OFF until glib itself installs its GIRs: g-ir-scanner
# needs GLib-2.0.gir, GObject-2.0.gir and Gio-2.0.gir, which only a glib built
# with introspection enabled writes, and the glib port builds without it.
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

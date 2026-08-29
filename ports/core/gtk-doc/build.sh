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

# IT IS HERE FOR ITS m4 MACRO, NOT FOR ITS DOCUMENTATION. A project of the
# GNOME lineage calls GTK_DOC_CHECK in configure.ac, and a MISSING m4 macro
# does not fail at aclocal time — it survives into the generated configure as a
# literal, and the shell reports `syntax error near unexpected token '1.8'`.
# The same failure shape as calcurse's AX_WITH_CURSES, which is why
# autoconf-archive is a port too.
#
# `gtk-doc.make` is the other half: an autotools project `include`s it and then
# appends to EXTRA_DIST, so it must exist AND define that variable even when
# --disable-gtk-doc means nothing in it will run.
#
# autotools_support=true is what installs the m4 and gtk-doc.make at all —
# with it off this port would be the scanner and nothing that needs it.
# Nothing here builds documentation: the scanner wants a full GObject
# introspection stack to be useful and this tree has none.
meson setup build --prefix=/usr --libdir=lib --buildtype=release \
	-Dautotools_support=true -Dcmake_support=false \
	-Dyelp_manual=false -Dtests=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

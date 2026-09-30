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

# One build, both APIs: libvte-2.91 for GTK 3 (Remmina, virt-manager, Geany's
# terminal tab) and libvte-2.91-gtk4. Their GIRs include Gtk-3.0, Gtk-4.0 and
# Pango-1.0, so both toolkits and pango must be built with introspection.
#
# simdutf and fast_float are built from the copies the release tarball carries
# under subprojects/; fmt is the fmt port, used for its headers only.
# --wrap-mode=nodownload lets meson use those directories and nothing else.
#
# _systemd=false: no libsystemd, so a spawned child is not moved into its own
# scope and no user unit drop-in is installed. The test terminal (app) and
# the Glade catalogue are not built.
#
# widget.cc builds a wait status with W_EXITCODE, which musl's <sys/wait.h>
# does not define; the patch supplies glibc's definition when it is missing.
patch -p1 -i "$PORT_SRC/fix-W_EXITCODE.patch"
meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	--wrap-mode=nodownload \
	-Dgtk3=true \
	-Dgtk4=true \
	-Dgir=true \
	-Dvapi=true \
	-Da11y=true \
	-Dfribidi=true \
	-Dgnutls=true \
	-Dicu=true \
	-D_systemd=false \
	-Dterminfo=false \
	-Dapp=false \
	-Dglade=false \
	-Ddocs=false \
	-Ddbg=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

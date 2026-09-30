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

# gtk3 carries both the Wayland and the X11 backend, so the X11 API is required
# rather than detected: a gtk3 built without X11 fails setup here instead of
# giving a gtkmm that silently lacks Gdk::X11 for the programs that call it.
meson setup build --prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	-Dmaintainer-mode=false \
	-Dwarnings=min \
	-Dbuild-deprecated-api=true \
	-Dbuild-documentation=false \
	-Dbuild-atkmm-api=true \
	-Dbuild-x11-api=true \
	-Dbuild-demos=false \
	-Dbuild-tests=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

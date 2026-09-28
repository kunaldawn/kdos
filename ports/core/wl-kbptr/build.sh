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

# The overlay is a wlr-layer-shell surface and the click goes out through
# zwlr_virtual_pointer_v1, so both globals must be offered by the compositor
# or the program exits without drawing. opencv is off: it drives the
# target-detection mode, which also needs wlr-screencopy, and no port
# provides opencv; the tile, floating, bisect, split and click modes do not
# use it.
meson setup build \
	--prefix=/usr --libdir=lib \
	--buildtype=release \
	-Dopencv=disabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

# NO MENU ENTRY: wl-kbptr is a one-shot overlay started from a key binding,
# and upstream's entry names no icon the panel can draw.
rm -f "$PKG/usr/share/applications/wl-kbptr.desktop"
rmdir "$PKG/usr/share/applications"

install -Dm644 config.example "$PKG/usr/share/doc/wl-kbptr/config.example"

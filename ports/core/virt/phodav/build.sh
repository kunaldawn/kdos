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

# -Davahi=enabled lets chezdav announce a share on the local network.
#
# The udev rule only starts spice-webdavd through SYSTEMD_WANTS, which nothing
# here reads, so it is installed to a fixed path and removed. The empty unit
# directory falls back to systemd.pc, which no port provides, so no unit is
# installed.
#
# asciidoc and xmlto build chezdav's manual page; with either missing the page
# is skipped silently.
meson setup build --prefix=/usr --libdir=lib --buildtype=release \
	-Dgtk_doc=disabled \
	-Davahi=enabled \
	-Dsystemdsystemunitdir= \
	-Dudevrulesdir=/usr/lib/udev/rules.d
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
rm -r "$PKG/usr/lib/udev"

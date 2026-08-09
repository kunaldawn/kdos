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

meson setup build \
	--prefix=/usr --libdir=lib --sysconfdir=/etc \
	--buildtype=release \
	-D i18n=disabled \
	-D docs=disabled \
	-D audit=disabled \
	-D econf=disabled \
	-D logind=disabled \
	-D elogind=disabled \
	-D selinux=disabled \
	-D nis=disabled \
	-D pam_userdb=disabled \
	-D examples=false \
	-D xtests=false \
	-D pam_unix=enabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

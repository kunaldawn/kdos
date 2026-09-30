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

# bwrap is installed WITHOUT the setuid bit. It then sandboxes through an
# unprivileged user namespace, which the kernel allows (CONFIG_USER_NS=y), and
# that is how xdg-desktop-portal runs its icon and sound validators. A setuid
# bwrap would be a twentieth setuid binary for no caller that needs one.
meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	-Dman=enabled \
	-Dselinux=disabled \
	-Dtests=false \
	-Dbash_completion=enabled \
	-Dzsh_completion=enabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

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

patch -p1 -i "$PORT_SRC/meson-version.patch"

muon setup \
	-Dprefix=/usr \
	-Dbuildtype=release \
	-Ddefault_library=both \
	-Dwith-pkg-config-dir=/usr/lib/pkgconfig:/usr/share/pkgconfig \
	-Dwith-system-libdir=/lib:/usr/lib \
	-Dwith-system-includedir=/usr/include \
	build
muon -C build samu
DESTDIR=$PKG muon -C build install

ln -sf pkgconf "$PKG"/usr/bin/pkg-config
ln -sf pkgconf.1 "$PKG"/usr/share/man/man1/pkg-config.1

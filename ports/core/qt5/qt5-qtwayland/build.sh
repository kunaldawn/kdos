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

patch -p1 -i "$PORT_SRC/0001-Update-wayland.xml-to-version-1.23.0.patch"

/usr/lib/qt5/bin/qmake -- \
	-feature-wayland-client \
	-feature-wayland-client-xdg-shell \
	-feature-wayland-egl \
	-feature-wayland-server \
	-no-feature-xcomposite-egl \
	-no-feature-xcomposite-glx
make
make INSTALL_ROOT="$PKG" install

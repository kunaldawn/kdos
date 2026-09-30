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


# wayland.xml follows libwayland 1.23: the generated client wrappers must
# describe the same interface versions as the system libwayland-client, or
# the build stops on a protocol that disagrees with its headers.
patch -p1 -i "$PORT_SRC/0001-Update-wayland.xml-to-version-1.23.0.patch"

# The client plugin is what puts a Qt 5 program on Wayland, with xdg-shell
# windows and EGL surfaces; the server half is the QtWaylandCompositor module.
# The XComposite buffer paths are off: they hand X11 pixmaps to an embedded
# compositor, and nothing here embeds one.
/usr/lib/qt5/bin/qmake -- \
	-feature-wayland-client \
	-feature-wayland-client-xdg-shell \
	-feature-wayland-egl \
	-feature-wayland-server \
	-no-feature-xcomposite-egl \
	-no-feature-xcomposite-glx
make
make INSTALL_ROOT="$PKG" install

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
	--prefix=/usr \
	--sysconfdir=/etc \
	--libexecdir=/usr/lib/$name \
	--libdir=/usr/lib \
	--localstatedir=/var \
	-D x11_autolaunch=disabled \
	-D doxygen_docs=disabled \
	-D xml_docs=disabled \
	-D system_pid_file=/run/dbus/pid \
	-D system_socket=/run/dbus/system_bus_socket
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

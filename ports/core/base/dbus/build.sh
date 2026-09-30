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
	-D systemd=disabled \
	-D launchd=disabled \
	-D kqueue=disabled \
	-D epoll=enabled \
	-D inotify=enabled \
	-D apparmor=disabled \
	-D selinux=disabled \
	-D libaudit=disabled \
	-D qt_help=disabled \
	-D ducktype_docs=disabled \
	-D modular_tests=disabled \
	-D doxygen_docs=disabled \
	-D xml_docs=enabled \
	-D system_pid_file=/run/dbus/pid \
	-D system_socket=/run/dbus/system_bus_socket
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
rm -rf $PKG/run

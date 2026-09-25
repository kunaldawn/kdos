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

# scons, not make, and every option is named because gpsd's default build turns
# on Qt bindings, an X11 test client and a systemd unit — all three are the
# hard rule here. The udev rules already grant the serial classes a receiver
# arrives on.
# dbus_export, bluez, usb and ncurses are named yes, but SConscript drops each
# one silently when its library is missing rather than failing, so it is the
# depends line that keeps them. manbuild=no installs the manual pages the
# release tarball already carries instead of re-rendering them through
# asciidoctor. magic_hat=no because musl has no sys/timepps.h, so kernel PPS
# is unavailable either way.
# ONE INVOCATION, build and install together. A second `scons install` with no
# options re-reads .scons-option-cache, which this build does not leave behind,
# and dies on the missing file AFTER everything has already linked.
scons prefix=/usr libdir=/usr/lib \
	systemd=no qt=no xgps=no \
	dbus_export=yes bluez=yes usb=yes ncurses=yes \
	manbuild=no magic_hat=no \
	python_libdir="$(python3 -c 'import sysconfig; print(sysconfig.get_path("purelib"))')" \
	--jobs=$(nproc) \
	install DESTDIR=$PKG

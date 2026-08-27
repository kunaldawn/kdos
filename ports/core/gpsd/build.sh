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
# ONE INVOCATION, build and install together. A second `scons install` with no
# options re-reads .scons-option-cache, which this build does not leave behind,
# and dies on the missing file AFTER everything has already linked.
scons prefix=/usr libdir=/usr/lib \
	systemd=no qt=no xgps=no \
	python_libdir=/usr/lib/python3/site-packages \
	--jobs=$(nproc) \
	install DESTDIR=$PKG

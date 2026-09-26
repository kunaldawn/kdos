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
# hard rule here. fs/etc/udev/rules.d/70-kdos-serial.rules grants the serial
# classes a receiver arrives on; the rules below are what start the daemon.
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

# A RECEIVER THAT IS PLUGGED IN STARTS gpsd, AND NOTHING ELSE DOES. No init
# script runs it at boot, so clients on localhost:2947 get connection refused
# until a receiver arrives. gpsd.hotplug hands the new node to gpsdctl, which
# adds it to the running daemon or launches one; gpsd daemonizes with setsid,
# so it outlives the udev event that started it. 01_udev.sh's coldplug replays
# the `add` of a receiver already attached at boot.
#
# UPSTREAM'S 25-gpsd.rules IS NOT INSTALLED (scons udev-install would). It
# matches the generic CP2102 (10c4:ea60) and ATEN (0557:2008) bridges, so
# gpsd would claim and probe the serial line of every ESP32 board and USB
# adapter plugged in. These are its GPS-specific entries only — receivers
# whose USB id names them as one, and the kernel's gnss class — with the gnss
# node given to dialout, the group gpsd drops privileges to. A receiver
# behind a generic bridge is added with `gpsdctl add /dev/ttyUSB0`.
install -Dm755 gpsd.hotplug "$PKG/usr/lib/udev/gpsd.hotplug"
install -d "$PKG/usr/lib/udev/rules.d"
cat > "$PKG/usr/lib/udev/rules.d/70-kdos-gpsd.rules" <<'RULES'
SUBSYSTEM=="gnss", ACTION=="add", GROUP="dialout", MODE="0660", RUN+="/usr/lib/udev/gpsd.hotplug"
SUBSYSTEM!="tty", GOTO="kdos_gpsd_end"
ATTRS{idVendor}=="067b", ATTRS{idProduct}=="aaa0", SYMLINK+="gps%n", RUN+="/usr/lib/udev/gpsd.hotplug"
ATTRS{idVendor}=="091e", ATTRS{idProduct}=="0003", SYMLINK+="gps%n", RUN+="/usr/lib/udev/gpsd.hotplug"
ATTRS{idVendor}=="0e8d", ATTRS{idProduct}=="3329", SYMLINK+="gps%n", RUN+="/usr/lib/udev/gpsd.hotplug"
ATTRS{idVendor}=="1163", ATTRS{idProduct}=="0100", SYMLINK+="gps%n", RUN+="/usr/lib/udev/gpsd.hotplug"
ATTRS{idVendor}=="1163", ATTRS{idProduct}=="0200", SYMLINK+="gps%n", RUN+="/usr/lib/udev/gpsd.hotplug"
ATTRS{idVendor}=="1546", ATTRS{idProduct}=="01a5", SYMLINK+="gps%n", RUN+="/usr/lib/udev/gpsd.hotplug"
ATTRS{idVendor}=="1546", ATTRS{idProduct}=="01a6", SYMLINK+="gps%n", RUN+="/usr/lib/udev/gpsd.hotplug"
ATTRS{idVendor}=="1546", ATTRS{idProduct}=="01a7", SYMLINK+="gps%n", RUN+="/usr/lib/udev/gpsd.hotplug"
ATTRS{idVendor}=="1546", ATTRS{idProduct}=="01a8", SYMLINK+="gps%n", RUN+="/usr/lib/udev/gpsd.hotplug"
ATTRS{idVendor}=="1546", ATTRS{idProduct}=="01a9", SYMLINK+="gps%n", RUN+="/usr/lib/udev/gpsd.hotplug"
ATTRS{interface}=="Telit Wireless Module Port", ATTRS{bInterfaceNumber}=="06", SYMLINK+="gps%n", RUN+="/usr/lib/udev/gpsd.hotplug"
LABEL="kdos_gpsd_end"
RULES

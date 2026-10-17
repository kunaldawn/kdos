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

DESTDIR="$PKG" scons prefix=/usr libdir=/usr/lib \
	systemd=no qt=no xgps=no \
	dbus_export=yes bluez=yes usb=yes ncurses=yes \
	manbuild=no magic_hat=no \
	python_libdir="$(python3 -c 'import sysconfig; print(sysconfig.get_path("purelib"))')" \
	--jobs="$KDOS_JOBS" \
	install
test -x "$PKG/usr/sbin/gpsd"
test -e "$PKG/usr/lib/libgps.so"

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

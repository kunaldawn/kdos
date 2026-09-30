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

# The GTK 3 module and libcanberra-gtk3 read window properties through Xlib;
# the patch returns early on a display that is not an X11 one, so a GTK 3
# application on Wayland plays its sounds instead of crashing in Xlib.
patch -p1 -i "$PORT_SRC/dont-assume-all-GdkDisplays-are-GdkX11Displays.patch"

# ALSA, PulseAudio and GStreamer are loaded as backend modules; PipeWire
# answers the first two. tdb, the sound-lookup cache, is not a port here, so it
# is off rather than left to detection. No systemd units: canberra-boot is
# installed and nothing starts it.
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib --disable-static \
	--enable-alsa --enable-pulse --enable-gstreamer --enable-null \
	--enable-udev --disable-oss --disable-tdb \
	--enable-gtk3 --disable-gtk \
	--with-systemdsystemunitdir=no --disable-lynx --disable-gtk-doc
make
# libtool relinks each backend module against the library as it installs
# it, and a parallel install can relink before the library is in place.
make -j1 DESTDIR=$PKG install

# The gtk-doc HTML reference ships prebuilt and installs whatever
# --disable-gtk-doc says; the package carries no HTML manual.
rm -rf "$PKG/usr/share/gtk-doc"

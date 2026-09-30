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

# ICE for gst-plugins-bad's webrtcbin, which links the library and also builds
# its transport from libnice's own nicesrc and nicesink elements: without
# -Dgstreamer=enabled webrtcbin refuses every call with "libnice elements are
# not available". The elements need only gstreamer's core libraries, so this
# port sits between gstreamer and gst-plugins-bad. UPnP port mapping is off:
# gupnp-igd is not a port. openssl supplies the HMACs that sign STUN messages.
meson setup build --prefix=/usr --libdir=lib --buildtype=release \
	-Dcrypto-library=openssl \
	-Dgstreamer=enabled \
	-Dgupnp=disabled \
	-Dintrospection=disabled \
	-Dgtk_doc=disabled \
	-Dexamples=disabled \
	-Dtests=disabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

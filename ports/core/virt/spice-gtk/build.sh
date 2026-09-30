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

# -Dcoroutine=gthread: musl has no getcontext/makecontext/swapcontext, so the
# ucontext backend the default picks fails at setup; gthread runs each
# coroutine on a thread of its own.
#
# -Dpolkit=disabled builds no spice-client-glib-usb-acl-helper, the helper
# installed setuid root (or with cap_fowner) that opens a USB device node for
# an unprivileged client. With no session tracking here its polkit action is
# never granted to an active user anyway. USB redirection then reaches only
# the devices whose nodes the user can already open, through the udev rules
# that give them a group. libcap-ng is used by nothing else.
#
# -Dwebdav=enabled shares a client folder with the guest through phodav
# (libphodav-3.0). -Dsmartcard=enabled passes a smartcard reader through
# with libcacard, which reaches the reader through pcsc-lite.
#
# The video stream decoders come from GStreamer at run time (playbin, and
# the jpeg, vpx and libav plugins); MJPEG, the stream QEMU sends by default,
# also has the built-in decoder, so a guest console works without a plugin.
#
# The two spice-common options keep its protocol manual and its test
# programs out of the build; neither is installed.
meson setup build --prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	-Dgtk=enabled \
	-Dwayland-protocols=enabled \
	-Degl=enabled \
	-Dwebdav=enabled \
	-Dbuiltin-mjpeg=true \
	-Dusbredir=enabled \
	-Dpolkit=disabled \
	-Dlibcap-ng=disabled \
	-Dusb-ids-path=/usr/share/hwdata/usb.ids \
	-Dcoroutine=gthread \
	-Dintrospection=enabled \
	-Dvapi=enabled \
	-Dlz4=enabled \
	-Dsasl=enabled \
	-Dopus=enabled \
	-Dsmartcard=enabled \
	-Dgtk_doc=disabled \
	-Dspice-common:manual=false \
	-Dspice-common:tests=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

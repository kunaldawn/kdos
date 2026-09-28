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

# The Xfce widget library Xfburn is built on. GTK 3 is built with both
# backends and so is this: --enable-x11 is what an Xfce program started under
# Xwayland needs, --enable-wayland the native path. The session-management
# client (libSM/libICE) goes with X11; nothing here runs an XSMP session
# manager, so it registers with nothing. startup-notification, libgtop and
# Glade are not ports and are named off; epoxy and gudev feed only the libgtop
# system-information pane. The keyboard-shortcut library is
# xfwm4's and xfce4-settings', neither of which is here.
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib --disable-static \
	--enable-x11 \
	--enable-wayland \
	--enable-libsm \
	--disable-startup-notification \
	--disable-glibtop \
	--disable-epoxy \
	--disable-gudev \
	--disable-gladeui2 \
	--disable-keyboard-library \
	--enable-introspection=no \
	--enable-vala=no \
	--disable-gtk-doc \
	--disable-tests
make
make DESTDIR=$PKG install

# Bundled data is English only; the library falls back to its source strings.
rm -rf "$PKG/usr/share/locale"

# xfce4-about's entry is an "About Xfce" row on a desktop that is not Xfce.
rm -f "$PKG/usr/share/applications/xfce4-about.desktop"

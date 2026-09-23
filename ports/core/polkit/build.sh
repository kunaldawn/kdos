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

# The polkitd account comes from postinstall.sh, because it is a property of
# the root being installed into. /etc/polkit-1/rules.d is fs/'s, and phase 1
# sets its owner and mode.
meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib --libexecdir=/usr/lib --localstatedir=/var \
	--buildtype=release \
	-Dauthfw=shadow \
	-Dsession_tracking=ConsoleKit \
	-Dintrospection=false \
	-Dgtk_doc=false \
	-Dman=true \
	-Dgettext=false \
	-Dexamples=false \
	-Dtests=false \
	-Dos_type=lfs
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

# Installed whatever the options: meson falls back to /usr/lib when no systemd
# is found, and nothing here reads units, sysusers.d or tmpfiles.d.
rm -rf "$PKG/usr/lib/systemd" "$PKG/usr/lib/sysusers.d" "$PKG/usr/lib/tmpfiles.d"

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
#
# polkit-agent-helper-1 checks a password through PAM. os_type=lfs makes
# /etc/pam.d/polkit-1 include the system-auth, system-account, system-password
# and system-session stacks the pam port ships. pam_prefix is named because the
# default is /usr/lib/pam.d, which Linux-PAM never reads (it reads /etc/pam.d
# and its vendor directory, /usr/share/pam): the service would fall through to
# `other` and be denied.
meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib --libexecdir=/usr/lib --localstatedir=/var \
	--buildtype=release \
	-Dauthfw=pam \
	-Dpam_prefix=/etc/pam.d \
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

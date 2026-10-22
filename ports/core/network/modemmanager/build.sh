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

meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib --localstatedir=/var \
	--buildtype=release \
	--auto-features=enabled \
	-Dudev=true \
	-Dudevdir=/lib/udev \
	-Ddbus_policy_dir=/usr/share/dbus-1/system.d \
	-Dsystemdsystemunitdir=no \
	-Dsystemd_suspend_resume=false \
	-Dpowerd_suspend_resume=false \
	-Dsystemd_journal=false \
	-Dpolkit=strict \
	-Dat_command_via_dbus=false \
	-Dbuiltin_plugins=false \
	-Dmbim=true \
	-Dqmi=true \
	-Dqrtr=true \
	-Dintrospection=true \
	-Dvapi=false \
	-Dman=true \
	-Dgtk_doc=false \
	-Dbash_completion=true \
	-Dexamples=false \
	-Dtests=false \
	-Dfuzzer=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

rm "$PKG/usr/share/dbus-1/system-services/org.freedesktop.ModemManager1.service"

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


# /etc/init.d/42_modemmanager starts ModemManager at boot, after polkitd and
# before NetworkManager. The D-Bus activation file it installs is never the
# path that starts it: a User=root activation hangs off the setuid launch helper.
meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib --localstatedir=/var \
	--buildtype=release \
	-Db_ndebug=if-release \
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

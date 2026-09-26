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


# THE PAM MODULE IS THE POINT. The daemon on its own enrols fingers and
# authenticates nothing; pam_fprintd is what `sudo`'s stack consults, and
# without it the reader works and unlocks nothing. It is the only consumer:
# the console login is shadow's, built without PAM, and the lock screen
# checks the shadow file through kdos-checkpass, so neither asks for a finger.
#
# systemd IS OFF: the daemon is started on demand over D-Bus activation,
# which runs it as root through dbus-daemon-launch-helper (dbus's
# postinstall.sh gives the helper the group the bus needs to run it).
#
# ITS polkit ACTIONS ARE allow_active, and no subject is ever active here, so
# enrolling and verifying are refused to the desktop user and granted to root:
# `sudo fprintd-enroll <user>` puts a finger on file. 50-kdos.rules does not
# grant them, because a finger enrolled with no password would be a way into
# `sudo` for anyone at an unlocked session.
#
# AND ITS sd-bus PROVIDER IS basu, which upstream offers as a `combo` choice
# beside libsystemd and libelogind. This is the same selection every other
# sd-bus consumer in this tree makes; leaving the default asks for
# libsystemd, which does not exist here and fails at setup rather than
# falling back.
meson setup build --prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	-Dpam=true \
	-Dpam_modules_dir=/usr/lib/security \
	-Dsystemd=false \
	-Dlibsystemd=basu \
	-Dman=true \
	-Dgtk_doc=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

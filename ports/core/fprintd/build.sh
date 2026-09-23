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
# authenticates nothing; pam_fprintd is what a login, a lock screen and
# `sudo` consult, and without it the reader works and unlocks nothing.
#
# systemd IS OFF: the daemon is started on demand over D-Bus activation,
# which is the mechanism this image already has.
#
# AND ITS sd-bus PROVIDER IS basu, which upstream offers as a `combo` choice
# beside libsystemd and libelogind. This is the same selection every other
# sd-bus consumer in this tree makes; leaving the default asks for
# libsystemd, which does not exist here and fails at setup rather than
# falling back.
meson setup build --prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	-Dpam_modules_dir=/usr/lib/security \
	-Dsystemd=false \
	-Dlibsystemd=basu \
	-Dman=true \
	-Dgtk_doc=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

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


# THE `systemd` MESON OPTION IS DEAD and must not be mistaken for the switch.
# 0.9.11 declares it `DEPRECATED` and reads it nowhere; the init-system branch
# is decided by `dependency('systemd', required: false)` at setup. With no
# systemd.pc on this image that probe fails, the unit file is not installed,
# and `install_emptydir` creates the device database directory the unit would
# otherwise have made. So the non-systemd path is what a plain setup already
# takes here, and passing -Dsystemd=false would change nothing while claiming
# to.
#
# NOTHING LINKS sd-bus EITHER. boltd talks D-Bus through GDBus, and its
# readiness and watchdog pings are its own `bolt_sd_notify_literal`, a write to
# $NOTIFY_SOCKET with no library behind it — unset here, so the calls are
# no-ops. basu is not a dependency and adding it would link nothing.
#
# --localstatedir=/var PINS THE DEVICE DATABASE AT /var/lib/boltd. meson
# derives localstatedir from the prefix and only /usr yields /var; the path is
# then frozen into the daemon as BOLT_DBDIR and substituted into the installed
# emptydir, so a prefix this recipe did not state would compile boltd to keep
# enrolled device keys somewhere else — /var/local/lib under /usr/local, and a
# path relative to the working directory under any other prefix.
#
# boltd IS INSTALLED STRAIGHT INTO libexecdir, with no subdirectory of its own,
# and the same string is substituted into the D-Bus service file's Exec=. The
# default /usr/libexec is therefore kept: overriding it to /usr/lib would drop
# a bare daemon among the shared libraries, and fprintd — the other
# D-Bus-activated system daemon here — already lives in /usr/libexec.
#
# -Dprivileged-group=wheel MATCHES /etc/group: the installed polkit rule grants
# enroll, authorize and manage to members of that group, and a name no account
# is in makes the rule unreachable and every authorisation an admin prompt.
#
# -Dman=true MAKES THE MAN PAGES A HARD REQUIREMENT. The default `auto` builds
# them when a2x happens to be found, which silently drops boltctl(1) and
# boltd(8) whenever asciidoc is not installed first; asciidoc is in `depends`
# precisely so this cannot happen quietly.
#
# -Dinstall-tests=false KEEPS THE TEST PROGRAMS OUT OF THE PACKAGE. They are
# compiled regardless — bolt has no option to skip building them — and the
# umockdev-only ones are simply not in the list, because that dependency is
# `required: false` and umockdev is not on this image.
meson setup build --prefix=/usr --sysconfdir=/etc --libdir=lib \
	--localstatedir=/var \
	--buildtype=release \
	-Dman=true \
	-Dinstall-tests=false \
	-Dprivileged-group=wheel
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

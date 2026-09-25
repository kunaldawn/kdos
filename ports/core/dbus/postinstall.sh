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

# Everything is written under PKG_ROOT, the root kpkgadd is installing into:
# --root and an A/B update install into a tree that is not the running one.
#
# The system bus runs as messagebus and starts every User=root service through
# dbus-daemon-launch-helper, which it can execute only through the helper's
# GROUP bit. kpkg rolls every package root:root, so the group upstream's
# install step gave it is gone by the time the package lands: without this the
# bus is refused with EACCES for each activation — wpa_supplicant, fprintd,
# fwupd, boltd, upower, ModemManager and NetworkManager's dispatcher never
# start, and the only trace is a Spawn.ExecFailed in the log.
#
# The group is messagebus's primary gid, read from the target's passwd as a
# number: chown resolves a name against the running root, not this one. chown
# clears the setuid bit, so the mode is set after it. 4110 is upstream's:
# nobody but root and the bus's group may run it, and nobody may read it.
root="${PKG_ROOT:-/}"
helper="$root/usr/lib/dbus/dbus-daemon-launch-helper"
[ -f "$helper" ] || exit 0
while IFS=: read -r n _ _ gid _; do
	[ "$n" = messagebus ] || continue
	chown "0:$gid" "$helper"
	chmod 4110 "$helper"
done < "$root/etc/passwd"

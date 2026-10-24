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

# The broker started as root drops to this account; without it mosquitto
# falls back to `nobody`, which every other daemon asking for an unprivileged
# account shares.
#
# Everything is written under PKG_ROOT, the root kpkgadd is installing into:
# --root and an A/B update install into a tree that is not the running one, and
# an account made in / instead would be missing from the system that needs it.
#
# The guard reads PKG_ROOT's files: getent answers for the running root, not
# the tree being installed, and would let useradd run against an account the
# target already has.
root="${PKG_ROOT:-/}"
grep -q '^mosquitto:' "$root/etc/group" 2>/dev/null || groupadd -R "$root" -r mosquitto
grep -q '^mosquitto:' "$root/etc/passwd" 2>/dev/null || \
	useradd -R "$root" -r -g mosquitto -d /var/lib/mosquitto -s /sbin/nologin mosquitto

# chown resolves a name against the running root's passwd, so the ids are read
# out of PKG_ROOT's. The account's home is where a configuration that turns
# persistence on keeps the broker's state, and the broker writes it as this
# account.
while IFS=: read -r n _ uid gid _; do
	[ "$n" = mosquitto ] || continue
	chown "$uid:$gid" "$root/var/lib/mosquitto"
done < "$root/etc/passwd"

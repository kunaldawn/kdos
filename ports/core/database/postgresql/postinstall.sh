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

# The server runs as this account and refuses root, and initdb writes the
# cluster as whoever runs it, so the account owns the data directory: a cluster
# owned by anyone else is one postgres refuses to open.
#
# Everything is written under PKG_ROOT, the root kpkgadd is installing into:
# --root and an A/B update install into a tree that is not the running one, and
# an account made in / instead would be missing from the system that needs it.
#
# The guard reads PKG_ROOT's files: getent answers for the running root, not
# the tree being installed, and would let useradd run against an account the
# target already has.
root="${PKG_ROOT:-/}"
grep -q '^postgres:' "$root/etc/group" 2>/dev/null || groupadd -R "$root" -r postgres
grep -q '^postgres:' "$root/etc/passwd" 2>/dev/null || \
	useradd -R "$root" -r -g postgres -d /var/lib/postgres -s /sbin/nologin postgres

# chown resolves a name against the running root's passwd, so the ids are read
# out of PKG_ROOT's.
# The mode is set as well: kpkgadd creates every directory 0755, whatever mode
# it was packaged with, because the package carries no group to give it.
while IFS=: read -r n _ uid gid _; do
	[ "$n" = postgres ] || continue
	chown "$uid:$gid" "$root/var/lib/postgres"
	chmod 700 "$root/var/lib/postgres"
done < "$root/etc/passwd"

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
# The guard reads the files: no getent on musl, and a missing one returns 127,
# so the test never holds and useradd runs against the account it already made.
root="${PKG_ROOT:-/}"
grep -q '^postgres:' "$root/etc/group" 2>/dev/null || groupadd -R "$root" -r postgres
grep -q '^postgres:' "$root/etc/passwd" 2>/dev/null || \
	useradd -R "$root" -r -g postgres -d /var/lib/postgres -s /sbin/nologin postgres

# chown resolves a name against the running root's passwd, so the ids are read
# out of PKG_ROOT's.
while IFS=: read -r n _ uid gid _; do
	[ "$n" = postgres ] || continue
	chown "$uid:$gid" "$root/var/lib/postgres"
done < "$root/etc/passwd"

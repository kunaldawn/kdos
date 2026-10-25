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
# --root and an A/B update install into a tree that is not the running one, and
# an account made in / instead would be missing from the system that needs it.
#
# The guard reads PKG_ROOT's files: getent answers for the running root, not
# the tree being installed, and would let useradd run against an account the
# target already has.
root="${PKG_ROOT:-/}"
grep -q '^mumble-server:' "$root/etc/group" 2>/dev/null || groupadd -R "$root" -r mumble-server
grep -q '^mumble-server:' "$root/etc/passwd" 2>/dev/null || \
	useradd -R "$root" -r -g mumble-server -d /var/lib/mumble-server -s /sbin/nologin mumble-server

# chown resolves a name against the running root's passwd, so the ids are read
# out of PKG_ROOT's. The ini is group-readable only, and the server reads it
# after setpriv has made it the account.
# The mode is set as well: kpkgadd creates every directory 0755, whatever mode
# it was packaged with, because the package carries no group to give it.
while IFS=: read -r n _ uid gid _; do
	[ "$n" = mumble-server ] || continue
	chown "$uid:$gid" "$root/var/lib/mumble-server"
	chmod 750 "$root/var/lib/mumble-server"
	chown "0:$gid" "$root/etc/mumble/mumble-server.ini"
done < "$root/etc/passwd"

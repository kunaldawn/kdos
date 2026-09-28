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
# The guard reads the files: no getent on musl, and a missing one returns 127,
# so the test never holds and useradd runs against the account it already made.
root="${PKG_ROOT:-/}"
grep -q '^radicale:' "$root/etc/group" 2>/dev/null || groupadd -R "$root" -r radicale
grep -q '^radicale:' "$root/etc/passwd" 2>/dev/null || \
	useradd -R "$root" -r -g radicale -d /var/lib/radicale -s /sbin/nologin radicale

# chown resolves a name against the running root's passwd, so the ids are read
# out of PKG_ROOT's.
while IFS=: read -r n _ uid gid _; do
	[ "$n" = radicale ] || continue
	chown "$uid:$gid" "$root/var/lib/radicale"
done < "$root/etc/passwd"

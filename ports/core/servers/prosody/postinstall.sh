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
grep -q '^prosody:' "$root/etc/group" 2>/dev/null || groupadd -R "$root" -r prosody
grep -q '^prosody:' "$root/etc/passwd" 2>/dev/null || \
	useradd -R "$root" -r -g prosody -d /var/lib/prosody -s /sbin/nologin prosody

# chown resolves a name against the running root's passwd, so the ids are read
# out of PKG_ROOT's.
while IFS=: read -r n _ uid gid _; do
	[ "$n" = prosody ] || continue
	chown "$uid:$gid" "$root/var/lib/prosody"
done < "$root/etc/passwd"

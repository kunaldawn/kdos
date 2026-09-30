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
# freshclam run as root switches to the clamav account (DatabaseOwner) and
# exits if it is missing; the database directory is that account's to write.
root="${PKG_ROOT:-/}"
grep -q '^clamav:' "$root/etc/group" 2>/dev/null || groupadd -R "$root" -r clamav
grep -q '^clamav:' "$root/etc/passwd" 2>/dev/null || \
	useradd -R "$root" -r -g clamav -d /var/lib/clamav -s /sbin/nologin clamav
uid=$(awk -F: '$1 == "clamav" { print $3 }' "$root/etc/passwd")
gid=$(awk -F: '$1 == "clamav" { print $3 }' "$root/etc/group")
[ -n "$uid" ] && [ -n "$gid" ] && chown "$uid:$gid" "$root/var/lib/clamav"

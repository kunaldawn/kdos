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

root="${PKG_ROOT:-/}"
grep -q '^colord:' "$root/etc/group" 2>/dev/null || groupadd -R "$root" -r colord
grep -q '^colord:' "$root/etc/passwd" 2>/dev/null || \
	useradd -R "$root" -r -g colord -d /var/lib/colord -s /sbin/nologin colord

while IFS=: read -r n _ uid gid _; do
	[ "$n" = colord ] || continue
	chown -R "$uid:$gid" "$root/var/lib/colord"
done < "$root/etc/passwd"

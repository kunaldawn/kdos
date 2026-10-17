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
grep -q '^kolibri:' "$root/etc/group" 2>/dev/null || groupadd -R "$root" -r kolibri
grep -q '^kolibri:' "$root/etc/passwd" 2>/dev/null || \
	useradd -R "$root" -r -g kolibri -d /var/lib/kolibri -s /sbin/nologin kolibri

install -d -m 750 "$root/var/lib/kolibri"
while IFS=: read -r n _ uid gid _; do
	[ "$n" = kolibri ] || continue
	chown "$uid:$gid" "$root/var/lib/kolibri"
done < "$root/etc/passwd"

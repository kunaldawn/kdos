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

# /etc/init.d/87_kolibri.sh runs Kolibri as this account, with its database,
# logs and channels in /var/lib/kolibri; with no account the service is
# skipped. Everything is written under PKG_ROOT, the root kpkgadd is
# installing into, and the guard reads the files: there is no getent on musl.
root="${PKG_ROOT:-/}"
grep -q '^kolibri:' "$root/etc/group" 2>/dev/null || groupadd -R "$root" -r kolibri
grep -q '^kolibri:' "$root/etc/passwd" 2>/dev/null || \
	useradd -R "$root" -r -g kolibri -d /var/lib/kolibri -s /sbin/nologin kolibri

# chown resolves a name against the running root's passwd, so the ids are read
# out of PKG_ROOT's.
install -d -m 750 "$root/var/lib/kolibri"
while IFS=: read -r n _ uid gid _; do
	[ "$n" = kolibri ] || continue
	chown "$uid:$gid" "$root/var/lib/kolibri"
done < "$root/etc/passwd"

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

# nm-openvpn-service hands openvpn --user/--group nm-openvpn and refuses every
# connection when either account is missing.
#
# Everything is written under PKG_ROOT, the root kpkgadd is installing into:
# --root and an A/B update install into a tree that is not the running one, and
# an account made in / instead would be missing from the system that needs it.
#
# The guard reads PKG_ROOT's files: getent answers for the running root, not
# the tree being installed, and would let useradd run against an account the
# target already has.
root="${PKG_ROOT:-/}"
grep -q '^nm-openvpn:' "$root/etc/group" 2>/dev/null || groupadd -R "$root" -r nm-openvpn
grep -q '^nm-openvpn:' "$root/etc/passwd" 2>/dev/null || \
	useradd -R "$root" -r -g nm-openvpn -d /var/lib/openvpn/chroot -s /sbin/nologin nm-openvpn

# openvpn is chrooted here as nm-openvpn. nm-openvpn-service passes --chroot
# only when the directory and its tmp/ exist and the account can write both;
# otherwise it runs openvpn unchrooted.
while IFS=: read -r n _ uid gid _; do
	[ "$n" = nm-openvpn ] || continue
	install -d -m 755 "$root/var/lib/openvpn/chroot/tmp"
	chown "$uid:$gid" "$root/var/lib/openvpn/chroot" "$root/var/lib/openvpn/chroot/tmp"
done < "$root/etc/passwd"

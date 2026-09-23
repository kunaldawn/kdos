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
# The guard reads the files: no getent on musl, and a missing one returns 127,
# so the test never holds and useradd runs against the account it already made.
root="${PKG_ROOT:-/}"
grep -q '^nm-openvpn:' "$root/etc/group" 2>/dev/null || groupadd -R "$root" -r nm-openvpn
grep -q '^nm-openvpn:' "$root/etc/passwd" 2>/dev/null || \
	useradd -R "$root" -r -g nm-openvpn -d /var/lib/openvpn/chroot -s /sbin/nologin nm-openvpn

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

# avahi-daemon drops to `avahi` and avahi-autoipd to `avahi-autoipd`. Both are
# created here rather than in fs/etc/passwd because 00_file_system.sh MERGES
# those files: a runtime-added service user survives a re-sync, a repo-added one
# would need the repo edited for every package that wants a uid.
#
# Everything is written under PKG_ROOT, the root kpkgadd is installing into:
# --root and an A/B update install into a tree that is not the running one, and
# an account made in / instead would be missing from the system that needs it.
#
# The guard reads the files: no getent on musl, and a missing one returns 127,
# so the test never holds and useradd runs against the account it already made.
root="${PKG_ROOT:-/}"
grep -q '^avahi:' "$root/etc/group" 2>/dev/null || groupadd -R "$root" -r avahi
grep -q '^avahi:' "$root/etc/passwd" 2>/dev/null || \
	useradd -R "$root" -r -g avahi -d /run/avahi-daemon -s /sbin/nologin avahi
grep -q '^avahi-autoipd:' "$root/etc/group" 2>/dev/null || \
	groupadd -R "$root" -r avahi-autoipd
grep -q '^avahi-autoipd:' "$root/etc/passwd" 2>/dev/null || \
	useradd -R "$root" -r -g avahi-autoipd -d /var/lib/avahi-autoipd \
		-s /sbin/nologin avahi-autoipd

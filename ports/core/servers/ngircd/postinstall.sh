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
grep -q '^ngircd:' "$root/etc/group" 2>/dev/null || groupadd -R "$root" -r ngircd
grep -q '^ngircd:' "$root/etc/passwd" 2>/dev/null || \
	useradd -R "$root" -r -g ngircd -d /nonexistent -s /sbin/nologin ngircd

# The guard leaves an existing account as it is, so the home is set apart
# from it: a tree that already has the account takes /nonexistent too. usermod
# exits 0 without writing when the home is already /nonexistent.
usermod -R "$root" -d /nonexistent ngircd

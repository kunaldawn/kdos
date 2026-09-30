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
# ccid's udev rule gives every CCID reader to the pcscd group; without the
# group, udev leaves the node root-only and a pcscd running as its own account
# sees no reader.
root="${PKG_ROOT:-/}"
grep -q '^pcscd:' "$root/etc/group" 2>/dev/null || groupadd -R "$root" -r pcscd
grep -q '^pcscd:' "$root/etc/passwd" 2>/dev/null || \
	useradd -R "$root" -r -g pcscd -d /run/pcscd -s /sbin/nologin pcscd
